#include "native_nv12_bgra.hpp"
#include <cstddef>
#include <cstdint>
#include <limits>
namespace scarlett::video {
namespace {
bool required(std::size_t rows,std::size_t pitch,std::size_t row_bytes,
              std::size_t& needed) noexcept {
    if(rows==0||pitch<row_bytes)return false;
    const std::size_t before=rows-1;
    if(before && pitch>(std::numeric_limits<std::size_t>::max()-row_bytes)/before)
        return false;
    needed=before*pitch+row_bytes;
    return true;
}
std::uint8_t clip(std::int32_t value) noexcept {
    if(value<0)return 0;
    if(value>255)return 255;
    return static_cast<std::uint8_t>(value);
}
}
Status convert(const Nv12& image,Bgra output,Matrix matrix) noexcept {
    if(!image.y || !image.uv || !output.pixels)return Status::NullBuffer;
    if(!image.width||!image.height||image.width>MaxDimension||
       image.height>MaxDimension)return Status::InvalidDimensions;
    if(matrix!=Matrix::Bt601Limited && matrix!=Matrix::Bt709Limited)
        return Status::UnknownMatrix;
    const auto width=static_cast<std::size_t>(image.width);
    const auto height=static_cast<std::size_t>(image.height);
    const std::size_t chroma_bytes=((width+1u)/2u)*2u;
    const std::size_t chroma_rows=(height+1u)/2u;
    const std::size_t rgba_bytes=width*4u;
    std::size_t need_y=0,need_uv=0,need_out=0;
    if(!required(height,image.y_pitch,width,need_y)||
       !required(chroma_rows,image.uv_pitch,chroma_bytes,need_uv)||
       !required(height,output.pitch,rgba_bytes,need_out))
        return Status::InvalidStride;
    if(image.y_bytes<need_y||image.uv_bytes<need_uv||output.bytes<need_out)
        return Status::BufferTooShort;

    // All geometry, stride and length checks complete before the first write.
    // Caller owns non-overlapping buffers throughout this synchronous call.
    for(std::size_t y=0;y<height;y++){
        const std::size_t yy=y*image.y_pitch;
        const std::size_t uv_row=(y/2u)*image.uv_pitch;
        const std::size_t dest=y*output.pitch;
        for(std::size_t x=0;x<width;x++){
            const std::int32_t luma=static_cast<std::int32_t>(image.y[yy+x]);
            const std::size_t chroma=uv_row+(x/2u)*2u;
            const std::int32_t u=static_cast<std::int32_t>(image.uv[chroma])-128;
            const std::int32_t v=static_cast<std::int32_t>(image.uv[chroma+1u])-128;
            const std::int32_t c=luma>16?luma-16:0;
            // ITU studio-range integer approximations (8-bit fixed point).
            const std::int32_t r=(298*c+(matrix==Matrix::Bt709Limited?459:409)*v+128)>>8;
            const std::int32_t g=(298*c-(matrix==Matrix::Bt709Limited?55:100)*u-
                                 (matrix==Matrix::Bt709Limited?136:208)*v+128)>>8;
            const std::int32_t b=(298*c+(matrix==Matrix::Bt709Limited?541:516)*u+128)>>8;
            auto* dst=output.pixels+dest+x*4u;
            dst[0]=clip(b);dst[1]=clip(g);dst[2]=clip(r);dst[3]=255u;
        }
    }
    return Status::Ok;
}
const char* status_name(Status value) noexcept {
    switch(value){
        case Status::Ok:return "ok";
        case Status::NullBuffer:return "null-buffer";
        case Status::InvalidDimensions:return "invalid-dimensions";
        case Status::InvalidStride:return "invalid-stride";
        case Status::BufferTooShort:return "buffer-too-short";
        case Status::UnknownMatrix:return "unknown-matrix";
    }
    return "unknown";
}
} // namespace scarlett::video
