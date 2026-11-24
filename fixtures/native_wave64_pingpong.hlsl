// Ping-pong scan: immutable source bank per step, one barrier per step.
// Original D3D12 equivalent of the portable integer compute fixture.
StructuredBuffer<uint4> inputLane : register(t0);
RWStructuredBuffer<uint> outputWord : register(u0);
groupshared uint4 scan[128];
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
    uint bank=0u;
    for(uint step=1u;step<64u;step*=2u) {
        uint next=bank^64u;
        uint4 a=scan[bank+lane], b=uint4(0u,0u,0u,0u);
        if(lane>=step) b=scan[bank+lane-step];
        scan[next+lane]=uint4(a.xy+b.xy,a.zw|b.zw);
        GroupMemoryBarrierWithGroupSync();
        bank=next;
    }
    uint valid=0u, value=0u;
    if(isLive && v.w<64u) { if(live[v.w]!=0u) {valid=1u;value=values[v.w];} }
    uint o=index*8u;
    outputWord[o]=isLive?1u:0u;
    outputWord[o+1u]=valid; outputWord[o+2u]=value;
    outputWord[o+3u]=isLive?scan[bank+lane].y-(pred?1u:0u):0u;
    outputWord[o+4u]=isLive?scan[bank+lane].x-v.x:0u;
    outputWord[o+5u]=isLive?scan[bank+63u].x:0u;
    outputWord[o+6u]=isLive?scan[bank+63u].z:0u;
    outputWord[o+7u]=isLive?scan[bank+63u].w:0u;
}
