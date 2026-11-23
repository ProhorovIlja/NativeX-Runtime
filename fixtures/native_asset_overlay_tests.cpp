// Portable original synthetic data-only mod-overlay tests; zero game assets.
#include "native_asset_overlay.hpp"
#include "native_asset_stream.hpp"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
using scarlett::overlay::Catalog;
using scarlett::overlay::Origin;
using scarlett::overlay::Status;
namespace fs=std::filesystem;
namespace {
void check(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
struct Sandbox {
    fs::path root,original,mods;
    Sandbox(){
        const auto t=std::chrono::steady_clock::now().time_since_epoch().count();
        root=fs::temp_directory_path()/("scarlett-overlay-fixture-"+std::to_string(t));
        original=root/"original";mods=root/"mods";
        fs::create_directories(original/"world");
        fs::create_directories(original/"script");
        fs::create_directories(mods/"pack-one"/"world");
        fs::create_directories(mods/"pack-two"/"world");
    }
    ~Sandbox(){std::error_code ec;fs::remove_all(root,ec);}
    void write(const fs::path& to,const std::string& contents)const{
        std::ofstream out(to,std::ios::binary|std::ios::trunc);
        check(out.is_open(),"synthetic test file creation");
        out<<contents;
        check(out.good(),"synthetic test file write");
    }
    std::string load(const fs::path& from)const{
        std::ifstream in(from,std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>());
    }
};
void originals_and_precedence(){
    Sandbox s;
    s.write(s.original/"world"/"foliage.dds","original-map");
    s.write(s.mods/"pack-one"/"world"/"foliage.dds","first-overwrite");
    s.write(s.mods/"pack-two"/"world"/"foliage.dds","second-overwrite");
    Catalog c(s.original,s.mods);
    const auto old=c.resolve("world/foliage.dds");
    check(old.status==Status::Ok && old.origin==Origin::Original &&
          s.load(old.path)=="original-map","read original as default");
    check(c.add("WORLD\\FOLIAGE.DDS","pack-one/world/foliage.dds")==Status::Ok,
          "Windows-style key normalization failed");
    check(c.add("world/foliage.dds","pack-two/world/foliage.dds")==Status::Ok,
          "two mods for same virtual path");
    const auto top=c.resolve("WoRlD/FOlIaGE.DDS");
    check(top.status==Status::Ok && top.origin==Origin::Override &&
          s.load(top.path)=="second-overwrite","latest layer did not win");
    scarlett::stream::Reader reader(1,4);
    const auto bytes=reader.submit({top.path,0,16,{}}).get();
    const std::string read(bytes.bytes.begin(),bytes.bytes.end());
    check(bytes.status==scarlett::stream::Status::Ok && read=="second-overwrite",
          "portable asset stream must read the selected override exactly");
    check(s.load(s.original/"world"/"foliage.dds")=="original-map",
          "non-destructive overlay changed original resource");
    c.clear();
    check(c.count()==0 && c.resolve("world/foliage.dds").origin==Origin::Original,
          "clear does not restore original fallback");
}
void malformed_virtual_paths(){
    Sandbox s;
    s.write(s.mods/"pack-one"/"world"/"foliage.dds","data");
    Catalog c(s.original,s.mods);
    const std::vector<std::string> bad={
        "","/world/foliage.dds","world/","world//a.dds","world/./a.dds",
        "world/../secret.dds","..\\secret.dds","C:\\windows\\a.dds",
        "\\\\server\\share\\file.dds","world/aux.txt","world/COM1.txt",
        "world/NUL","world/name.","world/name ","world/ na.me",
        "world/a:b.dds","world/x?.dds",std::string(241,'a'),
        "world/"+std::string(81,'x')+".dds"
    };
    for(const auto& name:bad){
        check(c.add(name,"pack-one/world/foliage.dds")==Status::InvalidPath,
              "invalid virtual path accepted");
        check(c.resolve(name).status==Status::InvalidPath,
              "invalid virtual path resolved");
    }
    check(c.count()==0,"failed path validation added an entry");
}
void reject_executables_and_missing_files(){
    Sandbox s;
    s.write(s.mods/"pack-one"/"world"/"foliage.dds","data");
    s.write(s.mods/"pack-one"/"world"/"fake.dll","never-load");
    Catalog c(s.original,s.mods);
    check(c.add("mods/launch.EXE","pack-one/world/foliage.dds")==Status::ExecutablePath,
          "executable virtual target accepted");
    check(c.add("world/texture.dds","pack-one/world/fake.dll")==Status::ExecutablePath,
          "executable mod payload accepted");
    check(c.resolve("bin/tool.asi").status==Status::ExecutablePath,
          "executable query accepted");
    check(c.add("world/missing.dds","pack-one/missing.dds")==Status::SourceMissing,
          "missing mod payload accepted");
    check(c.resolve("world/missing.dds").status==Status::SourceMissing,
          "missing original was fabricated");
    check(c.add("world/folder.dds","pack-one/world")==Status::NotRegularFile,
          "directory was accepted as a file payload");
}
void broken_override_fails_closed(){
    Sandbox s;
    s.write(s.original/"world"/"foliage.dds","original-safe");
    s.write(s.mods/"pack-one"/"world"/"foliage.dds","override-safe");
    Catalog c(s.original,s.mods);
    check(c.add("world/foliage.dds","pack-one/world/foliage.dds")==Status::Ok,
          "initial mod payload not accepted");
    fs::remove(s.mods/"pack-one"/"world"/"foliage.dds");
    const auto missing=c.resolve("world/foliage.dds");
    check(missing.status==Status::SourceMissing && missing.path.empty() &&
          missing.origin==Origin::Override,
          "deleted mod silently fell back to original");
    // Windows symbolic-link creation may require Developer Mode/privileges.
    std::error_code ec;
    fs::create_symlink(s.original/"world"/"foliage.dds",
                       s.mods/"pack-one"/"world"/"foliage.dds",ec);
    if(!ec){
        const auto unsafe=c.resolve("world/foliage.dds");
        check(unsafe.status==Status::SymlinkRejected && unsafe.path.empty(),
              "path-swap symlink escaped override root");
    }
}
void bounded_registration_and_source_paths(){
    Sandbox s;
    s.write(s.mods/"pack-one"/"world"/"foliage.dds","same-source");
    Catalog c(s.original,s.mods);
    check(c.add("world/foliage.dds","../outside.dds")==Status::InvalidPath,
          "source path parent traversal accepted");
    for(std::size_t i=0;i<Catalog::MaxEntries;i++)
        check(c.add("world/foliage.dds","pack-one/world/foliage.dds")==Status::Ok,
              "premature catalog capacity");
    check(c.count()==Catalog::MaxEntries &&
          c.add("world/foliage.dds","pack-one/world/foliage.dds")==Status::CapacityReached,
          "unbounded mod list accepted");
    check(c.resolve("world/foliage.dds").status==Status::Ok,
          "bounded catalog cannot resolve its last layer");
}
}
int main(){
    try{
        originals_and_precedence();
        malformed_virtual_paths();
        reject_executables_and_missing_files();
        broken_override_fails_closed();
        bounded_registration_and_source_paths();
    }catch(const std::exception& e){
        std::cerr<<"native read-only asset overlay FAIL: "<<e.what()<<'\n';return 1;
    }
    std::cout<<"{\"kind\":\"native-read-only-asset-overlay\","
             "\"scenarios\":5,\"synthetic_assets_only\":true,"
             "\"originals_unchanged\":true,\"portable_reader_integrated\":true,"
             "\"game_mod_loader_installed\":false}\n";
    return 0;
}
