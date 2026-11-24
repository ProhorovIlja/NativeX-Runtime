#include "native_asset_decode.hpp"
#include <algorithm>
#include <memory>
#include <zstd.h>

namespace scarlett::assetdecode {
namespace {
constexpr std::size_t MaxOutput=64u*1024u*1024u;
constexpr std::size_t Slice=64u*1024u;
Decoded fail(Status status){return {status,{}};}
bool stopped(const std::shared_ptr<std::atomic_bool>& token){return token&&token->load();}
}
Decoded zstd_frame(const stream::Result& input,std::size_t expected,Limits limits,
                   const std::shared_ptr<std::atomic_bool>& cancelled) {
    if(stopped(cancelled))return fail(Status::Cancelled);
    if(input.status!=stream::Status::Ok)return fail(Status::ReadFailed);
    if(!limits.output_bytes||limits.output_bytes>MaxOutput||
       limits.window_log_max<10||limits.window_log_max>27)return fail(Status::InvalidLimits);
    if(expected>limits.output_bytes)return fail(Status::OutputBudgetExceeded);
    const auto& data=input.bytes;
    if(data.size()<4||data.size()>stream::MaxRequestBytes||
       data[0]!=0x28||data[1]!=0xb5||data[2]!=0x2f||data[3]!=0xfd)
        return fail(Status::InvalidInput);
    if(input.checksum!=stream::fnv1a64(data))return fail(Status::InputChecksumMismatch);
    if(stopped(cancelled))return fail(Status::Cancelled);
    const auto declared=ZSTD_getFrameContentSize(data.data(),data.size());
    if(declared==ZSTD_CONTENTSIZE_ERROR||ZSTD_getDictID_fromFrame(data.data(),data.size())!=0)
        return fail(Status::InvalidInput);
    if(declared!=ZSTD_CONTENTSIZE_UNKNOWN&&declared!=expected)return fail(Status::SizeMismatch);
    using Context=std::unique_ptr<ZSTD_DCtx,decltype(&ZSTD_freeDCtx)>;
    Context ctx(ZSTD_createDCtx(),ZSTD_freeDCtx);
    if(!ctx||ZSTD_isError(ZSTD_DCtx_setParameter(ctx.get(),ZSTD_d_windowLogMax,limits.window_log_max)))
        return fail(Status::DecodeFailed);
    // Zstd's streaming API enforces the window policy; output is pre-sized
    // from validated external metadata, never grown from untrusted frame data.
    Decoded result{Status::Ready,std::vector<std::uint8_t>(expected)};
    ZSTD_inBuffer in{data.data(),data.size(),0};
    std::size_t written=0;
    std::uint8_t overflow_probe=0;
    for(;;){
        if(stopped(cancelled))return fail(Status::Cancelled);
        const auto remaining=expected-written;
        ZSTD_outBuffer out{remaining?result.bytes.data()+written:&overflow_probe,
                           remaining?std::min(remaining,Slice):1u,0};
        const auto previous=in.pos;
        const auto rc=ZSTD_decompressStream(ctx.get(),&out,&in);
        if(ZSTD_isError(rc))return fail(Status::DecodeFailed);
        if(out.pos>remaining)return fail(Status::SizeMismatch);
        written+=out.pos;
        if(rc==0){
            if(in.pos!=in.size)return fail(Status::InvalidInput);
            if(written!=expected)return fail(Status::SizeMismatch);
            if(stopped(cancelled))return fail(Status::Cancelled);
            return result;
        }
        // A non-complete frame without progress cannot be completed by this
        // finite input. Do not retry forever or expose partial output.
        if(in.pos==previous&&out.pos==0)return fail(Status::DecodeFailed);
    }
}
} // namespace scarlett::assetdecode
