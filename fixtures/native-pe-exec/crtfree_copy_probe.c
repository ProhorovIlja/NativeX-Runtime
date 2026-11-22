// Original CRT-free x64 PE32+ memory-copy and BSS/RDATA load acceptance.
// Uses MSVC __movsb intrinsic; no memcpy/memset import or CPU emulation.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <intrin.h>
#ifdef _MSC_VER
#pragma intrinsic(__movsb)
#endif
#define CopyBytes 8192u
static unsigned char source_page[CopyBytes]; // .bss
static unsigned char target_page[CopyBytes]; // .bss
static const unsigned char seed[16] = {
    11,22,33,44,55,66,77,88,99,110,121,132,143,154,165,176
};
int WINAPI ProbeEntry(void){
    unsigned int i;
    unsigned int observed=0;
    if(source_page[0]!=0 || target_page[CopyBytes-1]!=0)return 141;
    for(i=0;i<16u;i++) {
        if(seed[i]!=(unsigned char)((i+1u)*11u))return 142;
    }
    for(i=0;i<CopyBytes;i++){
        source_page[i]=(unsigned char)((i*29u+7u)%251u);
        target_page[i]=0u;
    }
    // MSVC guarantees the x64 byte-string intrinsic emits an inline
    // CPU REP MOVSB sequence, not an implicitly linked C runtime memcpy.
    __movsb(target_page,source_page,CopyBytes);
    for(i=0;i<CopyBytes;i++){
        const unsigned char expected=(unsigned char)((i*29u+7u)%251u);
        if(target_page[i]!=expected || source_page[i]!=expected)return 143;
        observed+=target_page[i];
    }
    if(observed==0u)return 144;
    // Validate a second nonoverlapping copy into the target's upper half.
    // The destination is kept inside the same mapped PE .bss page region.
    __movsb(target_page+4096u,source_page,4096u);
    for(i=0;i<4096u;i++){
        if(target_page[4096u+i]!=source_page[i])return 145;
    }
    return 47;
}
