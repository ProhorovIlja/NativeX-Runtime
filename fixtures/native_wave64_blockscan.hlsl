// Two-level block scan: independent 8-lane blocks, two group barriers.
// Original D3D12 equivalent of the portable integer compute fixture.
StructuredBuffer<uint4> inputLane : register(t0);
RWStructuredBuffer<uint> outputWord : register(u0);
groupshared uint4 scan[64];
groupshared uint4 totals[8];
groupshared uint values[64];
groupshared uint live[64];
[numthreads(64,1,1)]
void main(uint lane : SV_GroupIndex, uint3 group : SV_GroupID) {
    uint index=group.x*64u+lane;
    uint4 v=inputLane[index];
    bool isLive=v.y!=0u, pred=isLive && v.z!=0u;
    values[lane]=v.x; live[lane]=isLive?1u:0u;
    uint bit=1u<<(lane&31u);
    scan[lane]=uint4(isLive?v.x:0u,pred?1u:0u,pred&&lane<32u?bit:0u,pred&&lane>=32u?bit:0u);
    GroupMemoryBarrierWithGroupSync();
    uint block=lane/8u;
    uint4 prefix=uint4(0u,0u,0u,0u), total=uint4(0u,0u,0u,0u);
    for(uint i=block*8u;i<=lane;++i) {
        uint4 v=scan[i];prefix=uint4(prefix.xy+v.xy,prefix.zw|v.zw);
    }
    if((lane&7u)==7u) totals[block]=prefix;
    GroupMemoryBarrierWithGroupSync();
    for(uint i=0u;i<8u;++i) {
        uint4 v=totals[i];total=uint4(total.xy+v.xy,total.zw|v.zw);
        if(i<block) prefix=uint4(prefix.xy+v.xy,prefix.zw|v.zw);
    }
    uint valid=0u, value=0u;
    if(isLive && v.w<64u) { if(live[v.w]!=0u) {valid=1u;value=values[v.w];} }
    uint o=index*8u;
    outputWord[o]=isLive?1u:0u;
    outputWord[o+1u]=valid; outputWord[o+2u]=value;
    outputWord[o+3u]=isLive?prefix.y-(pred?1u:0u):0u;
    outputWord[o+4u]=isLive?prefix.x-v.x:0u;
    outputWord[o+5u]=isLive?total.x:0u;
    outputWord[o+6u]=isLive?total.z:0u;
    outputWord[o+7u]=isLive?total.w:0u;
}
