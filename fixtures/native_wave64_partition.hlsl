// D3D12 counterpart; independent logical-wave partition fixture.
StructuredBuffer<uint4> inputLane : register(t0);
RWStructuredBuffer<uint> outputWord : register(u0);
groupshared uint4 lanes[64];
groupshared uint reps[64];
[numthreads(64,1,1)]
void main(uint lane : SV_GroupIndex, uint3 group : SV_GroupID) {
    uint index=group.x*64u+lane;
    uint4 v=inputLane[index]; // value, active, partition key, source lane
    lanes[lane]=v;
    GroupMemoryBarrierWithGroupSync();
    bool live=v.y!=0u;
    uint first=64u, sum=0u, lo=0u, hi=0u;
    if(live) {
        for(uint i=0u;i<64u;++i) {
            if(lanes[i].y!=0u && lanes[i].z==v.z) {
                first=min(first,i);sum+=lanes[i].x;
                if(i<32u) lo|=1u<<i; else hi|=1u<<(i-32u);
            }
        }
    }
    reps[lane]=first;
    GroupMemoryBarrierWithGroupSync();
    uint order=0u, groups=0u, total=0u;
    for(uint i=0u;i<64u;++i) {
        if(reps[i]==i) {++groups;if(i<first) ++order;}
        if(lanes[i].y!=0u) total+=lanes[i].x;
    }
    uint valid=0u, value=0u, joinedValid=0u, joinedValue=0u;
    if(live && v.w<64u) {
        uint4 src=lanes[v.w];
        if(src.y!=0u) {
            joinedValid=1u;joinedValue=src.x;
            if(src.z==v.z) {valid=1u;value=src.x;}
        }
    }
    uint o=index*12u;
    outputWord[o]=live?1u:0u;
    outputWord[o+1u]=valid;outputWord[o+2u]=value;
    outputWord[o+3u]=first;outputWord[o+4u]=sum;
    outputWord[o+5u]=lo;outputWord[o+6u]=hi;
    outputWord[o+7u]=live?order:64u;
    outputWord[o+8u]=joinedValid;outputWord[o+9u]=joinedValue;
    outputWord[o+10u]=live?total:0u;outputWord[o+11u]=groups;
}
