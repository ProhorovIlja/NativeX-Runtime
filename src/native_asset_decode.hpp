// Original bounded single-frame decoder for independently produced assets.
#pragma once
#include "native_asset_stream.hpp"

namespace scarlett::assetdecode {
enum class Status { Ready, ReadFailed, InvalidLimits, InvalidInput, SizeMismatch,
                    OutputBudgetExceeded, InputChecksumMismatch, Cancelled, DecodeFailed };
struct Limits {
    std::size_t output_bytes=16u*1024u*1024u;
    int window_log_max=23;
};
struct Decoded {
    Status status=Status::DecodeFailed;
    std::vector<std::uint8_t> bytes;
};
// One standard Zstandard frame only; no dictionaries or trailing frames.
// expected_bytes is supplied independently of the frame's content-size field.
// Output limit applies per call, not to all concurrent decoders or scratch RAM.
Decoded zstd_frame(const stream::Result& input,std::size_t expected_bytes,
                   Limits limits={},const std::shared_ptr<std::atomic_bool>& cancelled={});
} // namespace scarlett::assetdecode
