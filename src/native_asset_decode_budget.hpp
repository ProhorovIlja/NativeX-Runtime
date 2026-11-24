// Original bounded shared ownership for decoded payloads; no game formats.
#pragma once
#include "native_asset_decode.hpp"

namespace scarlett::assetdecode {
struct DecodeStats {
    std::size_t committed_bytes=0, peak_bytes=0, byte_limit=0;
    std::size_t active=0, peak_active=0, active_limit=0;
    std::uint64_t rejected=0;
};
// Synchronous caller-side stage. Reserve expected output before decoding,
// retain the reservation until the last result owner releases it. Separate
// caps for payload and simultaneous decoder calls; codec scratch is excluded.
class BudgetedDecoder {
public:
    explicit BudgetedDecoder(std::size_t byte_limit,std::size_t active_limit=4);
    std::shared_ptr<const Decoded> decode(const stream::Result& input,
       std::size_t expected,Limits limits={},
       const std::shared_ptr<std::atomic_bool>& cancelled={});
    DecodeStats stats() const;
private:
    struct State;
    struct Charge;
    struct Owner;
    std::shared_ptr<State> state_;
};
} // namespace scarlett::assetdecode
