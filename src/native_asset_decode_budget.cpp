#include "native_asset_decode_budget.hpp"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace scarlett::assetdecode {
struct BudgetedDecoder::State {
    mutable std::mutex mutex;
    DecodeStats stats;
};
struct BudgetedDecoder::Charge {
    std::shared_ptr<State> state;
    std::size_t bytes;
    bool active=true;
    Charge(std::shared_ptr<State> s,std::size_t n) noexcept:state(std::move(s)),bytes(n){}
    Charge(const Charge&)=delete;
    Charge& operator=(const Charge&)=delete;
    void done() {
        std::lock_guard<std::mutex> lock(state->mutex);
        --state->stats.active;active=false;
    }
    ~Charge(){
        std::lock_guard<std::mutex> lock(state->mutex);
        if(active)--state->stats.active;
        state->stats.committed_bytes-=bytes;
    }
};
struct BudgetedDecoder::Owner {
    // Reverse destruction order frees payload before returning credit.
    std::unique_ptr<Charge> charge;
    Decoded result;
    Owner(std::unique_ptr<Charge> c,Decoded&& d) noexcept
        :charge(std::move(c)),result(std::move(d)){}
};
namespace {
std::shared_ptr<const Decoded> fail(Status status) {
    return std::make_shared<const Decoded>(Decoded{status,{}});
}
}
BudgetedDecoder::BudgetedDecoder(std::size_t byte_limit,std::size_t active_limit)
    :state_(std::make_shared<State>()) {
    if(!byte_limit||!active_limit||active_limit>64)
        throw std::invalid_argument("invalid shared decode limits");
    state_->stats.byte_limit=byte_limit;state_->stats.active_limit=active_limit;
}
std::shared_ptr<const Decoded> BudgetedDecoder::decode(const stream::Result& input,
    std::size_t expected,Limits limits,const std::shared_ptr<std::atomic_bool>& cancelled) {
    if(cancelled&&cancelled->load())return fail(Status::Cancelled);
    std::unique_ptr<Charge> ticket;
    {
        std::lock_guard<std::mutex> lock(state_->mutex);
        auto& s=state_->stats;
        if(expected>s.byte_limit-s.committed_bytes||s.active>=s.active_limit){
            ++s.rejected;return fail(Status::OutputBudgetExceeded);
        }
        // Allocate the nonthrowing RAII ticket before increasing counters.
        ticket=std::make_unique<Charge>(state_,expected);
        s.committed_bytes+=expected;++s.active;
        s.peak_bytes=std::max(s.peak_bytes,s.committed_bytes);
        s.peak_active=std::max(s.peak_active,s.active);
    }
    auto decoded=zstd_frame(input,expected,limits,cancelled);
    ticket->done();
    if(decoded.status!=Status::Ready)return fail(decoded.status);
    // If make_shared fails, the local decoded payload is destroyed before
    // the local ticket, restoring both counters safely.
    auto owner=std::make_shared<Owner>(std::move(ticket),std::move(decoded));
    return std::shared_ptr<const Decoded>(owner,&owner->result);
}
DecodeStats BudgetedDecoder::stats() const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->stats;
}
} // namespace scarlett::assetdecode
