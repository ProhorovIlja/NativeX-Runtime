// Original HLSL SM6 wave-intrinsic probe (no console shaders).
struct WaveRecord { uint lane; uint count; uint leader; uint sum; };
RWStructuredBuffer<WaveRecord> result : register(u0);
[numthreads(64,1,1)]
void main(uint3 id : SV_DispatchThreadID) {
    WaveRecord r;
    r.lane=WaveGetLaneIndex();
    r.count=WaveGetLaneCount();
    r.leader=WaveReadLaneAt(id.x,0u);
    r.sum=WaveActiveSum(id.x+1u);
    result[id.x]=r;
}
