# Upstream technical audit — 2026-11-22

Baseline: `sources/upstreams.json` (pinned 2026-11-21). Seven of the 14 public branch heads changed when last queried. The entries below are **observed upstream commits and author claims**, not independently reproduced gameplay.

| Upstream | Observed head | Material finding | Verification gap |
|---|---|---|---|
| [omonkeyz/xbox-emulator](https://github.com/omonkeyz/xbox-emulator/commit/0fc72956f9ff742a275534c27d9e5a7a9886a0c3) | `0fc72956` | ISA execution-test sweep and SDWA floating modifiers; author reports 169 tests | Independent clean build and hardware execution still needed |
| [drycheetah84/TomBox](https://github.com/drycheetah84/TomBox/commit/991903d7c010762a9e71e134f0ee55e7144ce3d0) | `b7137990` | GFX10 vector comparison decode fix; new diagnostic PRs | Clean clone and tests not reproduced here |
| [ivall/xcl](https://github.com/ivall/xcl/commit/fd746622e95dcb3a4b6b8a985ea045a1a3c6610a) | `18cb4004` | Physical pages mapped to reserved VA and synthetic streamed texture tests | CPU/GPU memory coherency not independently reproduced |
| [R0Y00/Xbox2Windows](https://github.com/R0Y00/Xbox2Windows/commit/cf77eda5df35c1acec538492fec0c1c59c31f18a) | `d311159b` | DXIL `dxil-ms-xb2` → `dxil-ms-dx` metadata retarget experiments | Metadata conversion does not prove shader output correctness |
| [ItsNotRexed/Leonida](https://github.com/ItsNotRexed/Leonida/commit/4e390e0871d7265e629b51aca8ff99a15a6a6845) | `4e390e08` | GPU test suite dependencies; graphics/runtime proposals | Significant PRs below were **open**, not merged |
| [realdody/americas-recomp](https://github.com/realdody/americas-recomp/tree/xbox-series) | `97e4d504` | Xbox-specific experimental branch moved | Exact diff not independently inspected |
| [brcly/TuxBox](https://github.com/brcly/TuxBox/commit/260e104cce6a9429277bb2a996520b0653cc0ad9) | `260e104c` | Other-title menu and shader experiments | No independently reproduced Windows title run |

## Particularly useful open PRs

- [Leonida #204](https://github.com/ItsNotRexed/Leonida/pull/204) reports AMD RADV native-wave64 `ds_bpermute` reading from unlaunched lanes in a partial 40-thread workgroup. The author proposes masking `OpGroupNonUniformShuffle` with a ballot and using zero for unavailable lanes. They report a fix on RX 6700 XT, not independent reproduction. The rule here is specific to the reported RDNA2 operation; **do not substitute it for HLSL WaveReadLaneAt's undefined inactive-lane behavior**.
- [Leonida #194](https://github.com/ItsNotRexed/Leonida/pull/194) introduces Vulkan swapchain and DXGI COM wrappers. Author reports 60 clear/present frames on RTX 4090. The PR was not connected to a real title's loader.
- [Leonida #203](https://github.com/ItsNotRexed/Leonida/pull/203) adds skip code 77 on missing fragment barycentrics/narrow subgroup requirements. No execution on a genuinely unsupported Vulkan device has been demonstrated in the PR body.
- [ivall/xcl #17](https://github.com/ivall/xcl/pull/17) proposes a bounded mesh shader two-wave32 dispatcher. The author reports focused tests but not real-program eligibility.

## Engineering interpretation

1. Keep native x86-64 Windows execution as the central architecture. Console-specific ABI/GDK/runtime, UMA and GPU semantics still require a compatibility layer.
2. Treat author claims, tested synthetic fixtures, a host graphics smoke test, and actual game execution as **different evidence levels**.
3. Focus independent tests on partial wave64, cross-half exchanges, memory aliasing/page remapping, DXIL transformation and D3D12 queue/command synchronisation.
4. No base has been selected, no Xbox or GTA VI game was launched in this repository, and no proprietary executables or shaders were imported.
5. Different upstream source licenses remain separate; do not merge GPL, LGPL, MIT and Apache files mechanically.

Review date: 2026-11-22. This is a point-in-time review; upstream branch states may change.
