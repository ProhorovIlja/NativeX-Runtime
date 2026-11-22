// Synthetic native Windows/Linux save-recovery acceptance, no game files.
#include "native_save_slots.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace scarlett::save;
namespace {
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
struct TempDirectory {
    std::filesystem::path root;
    TempDirectory(){
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        root=std::filesystem::temp_directory_path()/
             ("scarlett-save-fixtures-"+std::to_string(stamp));
        if(!std::filesystem::create_directory(root))
            throw std::runtime_error("temp folder name collision");
    }
    ~TempDirectory(){
        std::error_code ec;
        std::filesystem::remove_all(root,ec);
    }
};
void corrupt_payload(const std::filesystem::path& path){
    std::fstream f(path,std::ios::binary|std::ios::in|std::ios::out);
    check(f.good(),"could not open snapshot for corruption fixture");
    f.seekp(40,std::ios::beg);
    const char corrupted='X';
    f.write(&corrupted,1);
    f.flush();
    check(f.good(),"could not overwrite synthetic snapshot payload");
}
void recovery_and_interruption(){
    TempDirectory tmp;
    Store store(tmp.root/"test saves with spaces");
    Snapshot snapshot{};
    snapshot.generation=42;
    snapshot.bytes={99,88};
    check(store.load(snapshot)==Status::NotFound &&
          snapshot.generation==0 && snapshot.bytes.empty(),
          "missing must reset caller state");
    const std::vector<std::uint8_t> first{'s','a','v','e','-','1'};
    const std::vector<std::uint8_t> second{'s','a','v','e','-','2'};
    const std::vector<std::uint8_t> third{'s','a','v','e','-','3'};
    check(store.commit(first,0)==Status::Ok,"commit generation one");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==1 && snapshot.bytes==first,"load generation one");
    check(store.commit(second,0)==Status::Conflict,
          "stale compare-and-swap generation accepted");
    check(store.commit(second,1,Fault::BeforePublish)==Status::Interrupted,
          "before-publish fault was not injected");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==1 && snapshot.bytes==first,
          "before-publish altered committed slot");
    check(!std::filesystem::exists(tmp.root/"test saves with spaces"/"save-stage.tmp"),
          "unpublished stage was not discarded");
    check(store.commit(second,1)==Status::Ok,"second generation");
    {
        Store reopened(tmp.root/"test saves with spaces");
        check(reopened.load(snapshot)==Status::Ok &&
              snapshot.generation==2 && snapshot.bytes==second,
              "second generation did not survive new Store instance");
    }
    // Deliberate corruption of the newest slot must fall back to the prior one.
    corrupt_payload(tmp.root/"test saves with spaces"/"slot-b.scp");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==1 && snapshot.bytes==first,
          "corrupt newest slot did not recover previous save");
    check(store.commit(second,1)==Status::Ok,
          "could not recover from single corrupted slot");
    check(store.commit(third,2,Fault::AfterPublish)==Status::Interrupted,
          "post-publish fault was not injected");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==3 && snapshot.bytes==third,
          "post-publish generation not observable");
    // Caller must re-read after ambiguous post-publish failure.
    check(store.commit(first,2)==Status::Conflict,
          "stale write after publish accepted");
    corrupt_payload(tmp.root/"test saves with spaces"/"slot-a.scp");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==2 && snapshot.bytes==second,
          "corrupt slot-a did not roll back to slot-b");
    corrupt_payload(tmp.root/"test saves with spaces"/"slot-b.scp");
    check(store.load(snapshot)==Status::Corrupt &&
          snapshot.generation==0 && snapshot.bytes.empty(),
          "double corruption was not reported fail-closed");
    check(store.commit(first,0)==Status::Corrupt,
          "store overwrote both invalid snapshots");
}
void size_zero_and_header_validation(){
    TempDirectory tmp;
    Store store(tmp.root/"slot-tests");
    std::vector<std::uint8_t> oversized(MaxPayloadBytes+1,0xa5);
    check(store.commit(oversized,0)==Status::TooLarge,
          "oversized payload accepted");
    Snapshot snapshot{};
    check(store.load(snapshot)==Status::NotFound,
          "oversized payload wrote unexpected snapshot");
    check(store.commit({},0)==Status::Ok,
          "empty snapshot is a valid state");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==1 && snapshot.bytes.empty(),
          "empty snapshot corrupt");
    std::vector<std::uint8_t> maximum(MaxPayloadBytes);
    for(std::size_t i=0;i<maximum.size();i++)
        maximum[i]=static_cast<std::uint8_t>((i*31u+7u)%251u);
    check(store.commit(maximum,1)==Status::Ok,
          "maximum bounded snapshot write failed");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==2 && snapshot.bytes==maximum,
          "max size did not roundtrip correctly");
    // Reject malformed/truncated bytes and never hand them to a game consumer.
    TempDirectory separate;
    std::filesystem::create_directory(separate.root/"bad");
    {
        std::ofstream malformed(separate.root/"bad"/"slot-a.scp",std::ios::binary);
        malformed.write("SC",2);
    }
    Store broken(separate.root/"bad");
    check(broken.load(snapshot)==Status::Corrupt &&
          snapshot.bytes.empty(),"truncated snapshot not rejected");
    check(broken.commit({},0)==Status::Corrupt,
          "corrupt only-slot overwritten");
}
void unreadable_slot_fails_closed_even_with_valid_peer(){
    TempDirectory tmp;
    const auto folder=tmp.root/"unreadable slot with spaces";
    Store store(folder);
    const std::vector<std::uint8_t> first{'g','e','n','1'};
    const std::vector<std::uint8_t> second{'g','e','n','2'};
    const std::vector<std::uint8_t> third{'g','e','n','3'};
    Snapshot snapshot{};
    check(store.commit(first,0)==Status::Ok,"initial snapshot for IO error test");
    // Directories under the reserved slot names are guaranteed not to be
    // regular snapshot files on either POSIX or Win32 file_size APIs.
    check(std::filesystem::create_directory(folder/"slot-b.scp"),
          "failed to create intentionally unreadable slot-b fixture");
    snapshot={999,{1,2,3}};
    check(store.load(snapshot)==Status::IoError &&
          snapshot.generation==0 && snapshot.bytes.empty(),
          "unreadable slot B was wrongly treated as absent or corrupt");
    check(store.commit(second,1)==Status::IoError,
          "valid old slot A improperly allowed write while slot B unreadable");
    check(!std::filesystem::exists(folder/"save-stage.tmp"),
          "failed IO generation check unexpectedly published stage");
    check(std::filesystem::remove(folder/"slot-b.scp"),
          "could not remove unreadable slot-b fixture");
    check(store.commit(second,1)==Status::Ok,
          "valid second generation did not recover after slot B restored");
    // Invert sides: newest B is valid, while older A becomes unreadable.
    check(std::filesystem::remove(folder/"slot-a.scp"),
          "could not remove old slot A");
    check(std::filesystem::create_directory(folder/"slot-a.scp"),
          "could not replace slot A with unreadable directory");
    check(store.load(snapshot)==Status::IoError &&
          snapshot.generation==0 && snapshot.bytes.empty(),
          "valid newest slot B hid IO failure in older slot A");
    check(store.commit(third,2)==Status::IoError,
          "writer accepted ambiguous old slot A");
    check(std::filesystem::remove(folder/"slot-a.scp"),
          "could not remove invalid directory slot A");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==2 && snapshot.bytes==second,
          "valid generation B not recoverable after IO error resolved");
    check(store.commit(third,2)==Status::Ok,
          "commit failed after clearing IO error on both slots");
    check(store.load(snapshot)==Status::Ok &&
          snapshot.generation==3 && snapshot.bytes==third,
          "generation progression incorrect after IO-error repair");
}
void explicit_statuses(){
    check(std::string(status_text(Status::NotFound))=="not-found","not-found label");
    check(std::string(status_text(Status::Conflict))=="generation-conflict",
          "conflict label");
}
}
int main(){
    try{
        recovery_and_interruption();
        size_zero_and_header_validation();
        unreadable_slot_fails_closed_even_with_valid_peer();
        explicit_statuses();
    }catch(const std::exception& e){
        std::cerr<<"native two-slot save test FAILED: "<<e.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-windows-linux-two-slot-recovery\","
             "\"scenarios\":4,"
             "\"two_generations\":true,"
             "\"stage_faults_tested\":true,"
             "\"corrupt_slot_fallback\":true,"
             "\"double_corruption_fails_closed\":true,"
             "\"generation_conflicts_rejected\":true,"
             "\"max_payload_bytes\":"<<MaxPayloadBytes<<","
             "\"actual_power_failure_tested\":false,"
             "\"gta_vi_save_format_tested\":false}\n";
    return 0;
}
