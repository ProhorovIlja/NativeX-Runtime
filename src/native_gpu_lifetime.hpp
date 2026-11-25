#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>
namespace scarlett::gpu_lifetime {
using Id=std::uint64_t;
constexpr Id Invalid=std::numeric_limits<Id>::max();
enum class Queue:std::size_t {Copy,Graphics,Compute};
enum class State {Reserved,Uploading,Active,Retired};
using Fences=std::array<Id,3>;
struct Record {Id token,slot,bytes;std::size_t offset,count;State state;Fences uses{};};
struct Stats {Id budget,charged,peak;std::size_t descriptors,entries;Fences open,submitted,completed;};
// Single owner, one shader-visible descriptor heap, one ordered fence per queue.
// Caller initializes resources/descriptors, registers all recorded uses BEFORE
// submitting, and closes each batch only after successful queue-side signaling.
// No GPU calls, interqueue waits, heap residency or implicit destruction.
// On failed signal or device loss, recover externally; never invent completion.
class Ledger {
    struct Slot {Id active=0,pending=0;};
    std::map<Id,Record> records_;
    std::map<Id,Slot> slots_;
    std::vector<Id> owners_;
    Id budget_,charged_=0,peak_=0,next_=1;
    std::size_t used_=0;
    const std::size_t limit_;
    Fences open_{},submitted_{},completed_{};
    static std::size_t checked(std::size_t n){if(!n||n>65536)throw std::invalid_argument("descriptor count");return n;}
    static std::size_t index(Queue q) noexcept {return static_cast<std::size_t>(q);}
    bool done(const Record& r) const noexcept {for(std::size_t i=0;i<3;++i)if(r.uses[i]>completed_[i])return false;return true;}
    void detach(const Record& r) noexcept {
        auto s=slots_.find(r.slot);if(s==slots_.end())return;
        if(s->second.active==r.token)s->second.active=0;
        if(s->second.pending==r.token)s->second.pending=0;
        if(!s->second.active&&!s->second.pending)slots_.erase(s);
    }
public:
    Ledger(Id budget,std::size_t descriptors,std::size_t limit=4096):owners_(checked(descriptors)),budget_(budget),limit_(limit){if(!limit||limit>65536)throw std::invalid_argument("entry limit");}
    Ledger(const Ledger&)=delete;Ledger& operator=(const Ledger&)=delete;
    Stats stats() const noexcept {return {budget_,charged_,peak_,used_,records_.size(),open_,submitted_,completed_};}
    void set_budget(Id b) noexcept {budget_=b;}
    std::optional<Record> inspect(Id token) const {auto i=records_.find(token);if(i==records_.end())return {};return i->second;}
    Id active(Id slot) const noexcept {auto i=slots_.find(slot);return i==slots_.end()?0:i->second.active;}
    std::optional<Record> reserve(Id slot,Id bytes,std::size_t count){
        if(!slot||!bytes||!count||count>owners_.size()-used_||records_.size()>=limit_||next_==Invalid||charged_>budget_||bytes>budget_-charged_)return {};
        auto s=slots_.find(slot);if(s!=slots_.end()&&s->second.pending)return {};
        std::size_t run=0;
        for(std::size_t i=0;i<owners_.size();++i){
            run=owners_[i]?0:run+1;if(run!=count)continue;
            Record r{next_,slot,bytes,i+1-count,count,State::Reserved,{}};
            auto inserted=records_.emplace(next_,r);
            try{slots_.try_emplace(slot).first->second.pending=next_;}catch(...){records_.erase(inserted.first);throw;}
            for(std::size_t j=r.offset;j<=i;++j)owners_[j]=next_;
            ++next_;used_+=count;charged_+=bytes;if(charged_>peak_)peak_=charged_;return r;
        }return {};
    }
    bool begin(Queue q,Id fence) noexcept {auto n=index(q);if(n>=3||open_[n]||fence==Invalid||fence<=submitted_[n])return false;open_[n]=fence;return true;}
    bool upload(Id token,Queue q) noexcept {auto n=index(q);auto i=records_.find(token);if(n>=3||!open_[n]||i==records_.end()||i->second.state!=State::Reserved)return false;i->second.state=State::Uploading;i->second.uses[n]=open_[n];return true;}
    bool use(Id token,Queue q) noexcept {auto n=index(q);auto i=records_.find(token);if(n>=3||q==Queue::Copy||!open_[n]||i==records_.end()||i->second.state!=State::Active)return false;i->second.uses[n]=open_[n];return true;}
    bool submit(Queue q,Id fence) noexcept {auto n=index(q);if(n>=3||!open_[n]||open_[n]!=fence)return false;submitted_[n]=fence;open_[n]=0;return true;}
    bool observe(Queue q,Id fence) noexcept {auto n=index(q);if(n>=3||fence==Invalid||fence<completed_[n]||fence>submitted_[n])return false;completed_[n]=fence;return true;}
    // Descriptors must already be initialized. Publication switches both the
    // resource and descriptor generation, never exposes an unfinished upload.
    bool publish(Id token) noexcept {
        auto i=records_.find(token);if(i==records_.end()||i->second.state!=State::Uploading||!done(i->second))return false;
        auto s=slots_.find(i->second.slot);if(s==slots_.end()||s->second.pending!=token)return false;
        if(s->second.active)records_.at(s->second.active).state=State::Retired;
        s->second.active=token;s->second.pending=0;i->second.state=State::Active;return true;
    }
    bool retire(Id token) noexcept {auto i=records_.find(token);if(i==records_.end()||i->second.state==State::Retired)return false;detach(i->second);i->second.state=State::Retired;return true;}
    bool can_release(Id token) const noexcept {auto i=records_.find(token);return i!=records_.end()&&i->second.state==State::Retired&&done(i->second);}
    // Only call AFTER destruction of the actual allocation. No recycling
    // until BOTH the texture and its descriptor references are finished.
    bool released(Id token) noexcept {if(!can_release(token))return false;auto i=records_.find(token);const auto r=i->second;for(std::size_t j=0;j<r.count;++j)owners_[r.offset+j]=0;used_-=r.count;charged_-=r.bytes;records_.erase(i);return true;}
};
}
