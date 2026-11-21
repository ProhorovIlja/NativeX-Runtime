// Original deterministic page-identity / virtual-slot *reference model*.
// No Xbox console runtime, physical memory mapping, guest code or GPU execution.
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <vector>

namespace {
constexpr std::size_t kPageSize = 64u * 1024u;

class PageModel {
    struct Page {
        std::array<std::uint8_t,kPageSize> bytes{};
        std::optional<std::size_t> slot;
    };
    std::vector<Page> pages_;
    std::vector<std::optional<std::size_t>> slots_;
    std::vector<std::array<std::uint8_t,kPageSize>> uploaded_;
    std::vector<bool> dirty_;
public:
    PageModel(std::size_t page_count,std::size_t slot_count)
       : pages_(page_count), slots_(slot_count), uploaded_(slot_count),
         dirty_(slot_count,true) {}
    bool map(std::size_t page,std::size_t slot) {
        if(page>=pages_.size()||slot>=slots_.size())return false;
        if(slots_[slot] && *slots_[slot]!=page)return false;
        if(pages_[page].slot && *pages_[page].slot!=slot){
            const std::size_t previous=*pages_[page].slot;
            slots_[previous].reset();
            dirty_[previous]=true;
        }
        pages_[page].slot=slot;
        slots_[slot]=page;
        dirty_[slot]=true;
        return true;
    }
    bool unmap(std::size_t page) {
        if(page>=pages_.size()||!pages_[page].slot)return false;
        const std::size_t slot=*pages_[page].slot;
        slots_[slot].reset();
        pages_[page].slot.reset();
        dirty_[slot]=true;
        return true;
    }
    bool write(std::size_t slot,std::size_t offset,std::uint8_t value) {
        if(slot>=slots_.size()||offset>=kPageSize||!slots_[slot])return false;
        pages_[*slots_[slot]].bytes[offset]=value;
        dirty_[slot]=true;
        return true;
    }
    std::uint8_t preview(std::size_t slot,std::size_t offset) const {
        if(slot>=slots_.size()||offset>=kPageSize)
            throw std::out_of_range("preview address");
        if(!slots_[slot])return 0; // Explicit texture-stage choice, not host CPU load.
        return pages_[*slots_[slot]].bytes[offset];
    }
    std::uint8_t uploaded(std::size_t slot,std::size_t offset) const {
        if(slot>=slots_.size()||offset>=kPageSize)
            throw std::out_of_range("uploaded address");
        return uploaded_[slot][offset];
    }
    std::size_t upload_dirty() {
        std::size_t changed=0;
        for(std::size_t slot=0;slot<slots_.size();slot++){
            if(!dirty_[slot])continue;
            if(slots_[slot])
                uploaded_[slot]=pages_[*slots_[slot]].bytes;
            else
                uploaded_[slot].fill(0);
            dirty_[slot]=false;
            ++changed;
        }
        return changed;
    }
    std::optional<std::size_t> location(std::size_t page) const {
        return page<pages_.size()?pages_[page].slot:std::nullopt;
    }
};
void expect(bool condition,const char* why) {
    if(!condition)throw std::runtime_error(why);
}
}
int main(){
    try {
        PageModel model(3,8);
        expect(model.upload_dirty()==8,"initial clear upload count");
        expect(model.upload_dirty()==0,"no unexpected upload");
        expect(model.map(0,3),"map page0 slot3");
        expect(model.map(1,6),"map page1 slot6");
        expect(model.map(2,1),"map page2 slot1 (out of order)");
        expect(model.write(3,0,0xA1),"write first byte");
        expect(model.write(3,kPageSize-1,0xB2),"write last byte");
        expect(model.write(1,13,0xC3),"write second physical page");
        expect(model.write(6,123,0xD4),"write third physical page");
        expect(model.upload_dirty()==3,"only dirty slots uploaded");
        expect(model.uploaded(3,0)==0xA1 && model.uploaded(3,kPageSize-1)==0xB2,
               "page0 initial upload values");
        expect(model.uploaded(1,13)==0xC3 && model.uploaded(6,123)==0xD4,
               "out-of-order page uploads");

        // No aliasing of one physical-page ID across two slots.
        expect(!model.map(1,3),"overlap should be rejected");
        expect(model.location(1)==std::optional<std::size_t>{6},"rejected remap mutated old slot");
        expect(!model.map(99,0) && !model.map(1,8),"invalid page/slot accepted");
        expect(!model.write(7,0,1) && !model.write(3,kPageSize,1),
               "write to unmapped/out-of-range address accepted");
        expect(model.upload_dirty()==0,"rejected writes dirtied a slot");

        // Moving the same page must retain bytes at both edges and clear old visible slot.
        expect(model.map(0,5),"remap page0 from slot3 to slot5");
        expect(model.preview(3,0)==0 && model.preview(5,0)==0xA1,
               "page contents did not move");
        expect(model.uploaded(3,0)==0xA1 && model.uploaded(5,0)==0,
               "GPU-staging snapshot changed before upload");
        expect(model.upload_dirty()==2,"both stale/new mapping slots must update");
        expect(model.uploaded(3,0)==0 && model.uploaded(5,0)==0xA1
               && model.uploaded(5,kPageSize-1)==0xB2,"post-move upload mismatch");

        // Unmap preserves physical storage. Remap elsewhere must see same bytes.
        expect(model.unmap(0),"unmap page0");
        expect(model.preview(5,0)==0,"unmapped preview must be zero");
        expect(model.upload_dirty()==1,"unmap invalidation count");
        expect(model.uploaded(5,0)==0,"unmapped staging should be zero");
        expect(model.map(0,2),"remap page0 to slot2");
        expect(model.preview(2,0)==0xA1 && model.preview(2,kPageSize-1)==0xB2,
               "unmap/remap lost page data");
        expect(model.upload_dirty()==1,"remap new slot invalidate");
        expect(model.uploaded(2,kPageSize-1)==0xB2,"remap staged data lost");

        // Do not silently alias or overwrite a separately mapped page.
        expect(!model.map(2,2),"overlap page2 with page0 should fail");
        expect(model.location(2)==std::optional<std::size_t>{1},"failed collision damaged page2");
        expect(model.write(2,0,0x7F),"mutate currently mapped page");
        expect(model.uploaded(2,0)==0xA1,"staging changed without explicit upload");
        expect(model.upload_dirty()==1 && model.uploaded(2,0)==0x7F,
               "write-watch invalidation failed");

        std::cout<<"{\"kind\":\"physical-page-cpu-reference\","
                 <<"\"page_size\":65536,"
                 <<"\"page_count\":3,\"virtual_slots\":8,"
                 <<"\"move_unmap_remap\":true,"
                 <<"\"stale_staging_invalidation\":true,"
                 <<"\"xbox_xmem_tested\":false,\"gpu_execution\":false}\n";
    } catch(const std::exception& e) {
        std::cerr<<"physical-page CPU model FAILED: "<<e.what()<<'\n';
        return 1;
    }
    return 0;
}
