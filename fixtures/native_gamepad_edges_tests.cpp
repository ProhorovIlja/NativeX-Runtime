#include "native_gamepad_edges.hpp"
#include <array>
#include <bitset>
#include <iostream>
#include <random>
#include <stdexcept>
using scarlett::input::PadEdges;
void check(bool ok){if(!ok)throw std::runtime_error("per-pad edges mismatch");}
int main(){
    PadEdges x;
    check(x.record(0,0,1)&&x.record(1,0,1));
    check(x.press[0]==1&&x.press[1]==1);
    check(x.record(0,1,0)&&x.release[0]==1&&x.release[1]==0);
    check(x.record(1,1,0)&&x.record(1,0,1));
    check(x.press[1]==1&&x.release[1]==1);
    check(!x.record(4,0,0xffff));x.clear();
    for(unsigned seed:{64u,4070u,20261125u}){
        std::mt19937 rng(seed);
        std::array<std::bitset<16>,4> held{},p{},r{};
        for(unsigned i=0;i<100000;++i){
            const auto slot=static_cast<std::size_t>(rng()%6);
            const auto next=static_cast<std::uint16_t>(rng());
            if(rng()%13==0){x.clear();p={};r={};}
            else if(slot<4){
                const auto before=static_cast<std::uint16_t>(held[slot].to_ulong());
                check(x.record(slot,before,next));
                for(std::size_t bit=0;bit<16;++bit){
                    const bool now=((next>>bit)&1u)!=0;
                    if(now&&!held[slot][bit])p[slot].set(bit);
                    if(!now&&held[slot][bit])r[slot].set(bit);
                    held[slot].set(bit,now);
                }
            }else check(!x.record(slot,0,next));
            for(std::size_t s=0;s<4;++s){check(x.press[s]==p[s].to_ulong());check(x.release[s]==r[s].to_ulong());}
        }
        std::cout<<"seed="<<seed<<" operations=100000 mismatches=0\n";
        x.clear();
    }
}
