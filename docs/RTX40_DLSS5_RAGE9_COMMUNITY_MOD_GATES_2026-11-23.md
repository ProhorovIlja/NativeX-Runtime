# Community RTX 40 DLSS and RAGE modding compatibility — 2026-11-23

## Correction to an official-SDK-only assessment

The absence of *official* NVIDIA SDK support on a particular GPU does not prove a community mod cannot run. Direct GitHub README/metadata review confirms working public **projects claiming** Ada/RTX 40 neural-rendering or DLSS feature unlocking. Independent September 2026 reporting physically exercised **RTX 40 Multi Frame Generation** and observed operation and visual artifacts; this corroborates *that MFG case*, not every neural-rendering mode or game.

Author-reported community projects inspected directly, with GitHub source/license metadata:

| Project and pinned revision | License/contract | Evidence and hard limit |
|---|---|---|
| [ShyVortex/dlss-unlocked](https://github.com/ShyVortex/dlss-unlocked) at [f1c1cd69](https://github.com/ShyVortex/dlss-unlocked/commit/f1c1cd691f2fab29c591fdab45e297b88cb8a91a) | GitHub MIT; may bundle or call separately licensed components | Describes DX12 hooking and a Neural Rendering / pre-SR multipass forwarder plus RTX 40 Ada MFG unlock. README **requires a game with native DLSS upscaling + FG support for key features**; not automatic GTA VI compatibility. **Linux Proton != true native ELF**. Author warns against online/anti-cheat titles. |
| [optiscaler/OptiScaler](https://github.com/optiscaler/OptiScaler) at [ff23c5e3](https://github.com/optiscaler/OptiScaler/commit/ff23c5e39e6f1ba58d46209bef4f5f1e1f4f7050) | GPL-3.0 | Replaces/translates SR/FG input APIs with separate renderer/API restrictions; DX11, DX12, Vulkan have different support. ASI plugin loading is optional and OFF by default. Warns against use in online games. **Do not copy GPL code into our MIT host**. |
| [KleberMotta/OptiScaler-DLSS5-MFG-RTX40](https://github.com/KleberMotta/OptiScaler-DLSS5-MFG-RTX40) at [74c3bac2](https://github.com/KleberMotta/OptiScaler-DLSS5-MFG-RTX40/commit/74c3bac29f115d3115a17fce9f6c5864645b69e8) | GPL-3.0 | Claims DLSS 5 Neural Rendering pass + RTX 40 MFG unlock; warns about provider build-specific patching, added latency, artifacts, and online incompatibility. **Game-specific and binary-version sensitive; not an established GTA VI performance improvement**. |
| [Coolaid003/dlss5-aio](https://github.com/Coolaid003/dlss5-aio) at [a7aab8b5](https://github.com/Coolaid003/dlss5-aio/commit/a7aab8b5035b38f77721345a0a83264bf580459c) | GitHub MIT for scripts; separate NVIDIA runtime redistribution/redistributable terms apply | Documents RTX 40/50 Neural Rendering via Swapper/Feeder, often requires ReShade + addons and external runtime DLLs. Such DLLs **do not acquire MIT licensing simply by appearing in a repository/archive**. No external game injection performed here. |
| [danzig666/RTX40MFG-minimal](https://github.com/danzig666/RTX40MFG-minimal) at [3eb54a62](https://github.com/danzig666/RTX40MFG-minimal/commit/3eb54a62013bd4da37e441c6ee6740b602d63756) | GitHub NOASSERTION; check exact source license before reuse | Minimal Ada Multi Frame Generation core; requires a working DLSS FG/Streamline pipeline and a compatible nvngx_dlssg 310.x provider. **This is MFG, not Neural Rendering or native GPU throughput**. |

External published test: [Tom's Hardware September 12, 2026 RTX 40 MFG test](https://www.tomshardware.com/pc-components/gpus/we-tested-dlss-multi-frame-generation-on-rtx-40-series-gpus-new-mod-brings-rtx-50-series-exclusive-feature-to-older-cards-and-it-really-works) reported operational MFG with high-multiplier artifacts. Source/report distinction: no corresponding independent physical RTX 4070 DLSS 5 Neural Rendering benchmark was reproduced for this project.

## Do not conflate DLSS components

1. **DLSS Super Resolution / DLAA** reconstructs image detail; RTX 4070 is a legitimate target for vendor-compatible Super Resolution with properly verified D3D12/Vulkan integration. Quality or native output can be user-selected; whether DLSS is faster depends on its actual GPU cost and the frame bottleneck.
2. **DLSS 5 Neural Rendering** is a separate 3D-guided appearance/lighting pass, with tone/structure strength controls, and may add substantial work. Turning its beauty/appearance sliders down **does not demonstrate** it is a superior or cheaper Super Resolution. Community RTX 40 paths are candidates for **isolated optional evaluation** only after complete game render input/output integration, licensing and driver verification.
3. **DLSS FG/MFG** creates additional displayed frames and may increase latency and visual artifacts. User's requested profile **forces FG OFF** even if a community unlock says available. Report original simulation/render FPS, not generated FPS.
4. **NVIDIA Reflex** is separate. Use only when an actual host integration with the chosen graphics backend and driver exists; unknown mod compatibility is not sufficient.

Do not bundle scraped/leaked proprietary DLLs, bypass online anti-cheat, auto-install injectors, modify an unknown executable on a trusted main PC, claim RTX 40 official certification, or assume a GPL fork can be merged into the MIT source tree. Optional user-provided provider paths must be opt-in, validated, reversible, offline-only, and version-checked. Use a clean fallback when unavailable.

## GTA/RAGE precedent is real, but version-specific

- [scripthookvdotnet/scripthookvdotnet](https://github.com/scripthookvdotnet/scripthookvdotnet) at [28b4dfde](https://github.com/scripthookvdotnet/scripthookvdotnet/commit/28b4dfde2982951bedf3b2a848ce1e3223c133c3) (Zlib) provides GTA V ASI/.NET scripting on top of the original Script Hook V. Its README explicitly describes breakages across GTA V executable version transitions. This proves third-party script extensibility **for GTA V**, not the same ABI in GTA VI.
- [alloc8or/gta5-nativedb-data](https://github.com/alloc8or/gta5-nativedb-data) at [424fb51b](https://github.com/alloc8or/gta5-nativedb-data/commit/424fb51b089049a9fbcebcc641500b1d44d255b4) catalogs GTA V native commands. **Do not assume RAGE 9 native hashes, calling convention, layout or script VM matches RAGE in GTA V.**
- [ItsNotRexed/Leonida](https://github.com/ItsNotRexed/Leonida) at [561b65a8](https://github.com/ItsNotRexed/Leonida/commit/561b65a832218fbb9e6d2effd392a8eef093a3f8) (GPL-3.0) and other GTA VI x64 projects describe title boot as unfinished: **a host compatibility layer is not itself a fully audited, editable game engine**.
- RPF9, TTPF, BCPack, ADAT/AWC and scene/mesh extraction tools reviewed in [public upstream audit](GTA_VI_PUBLIC_UPSTREAM_NATIVE_PERF_2026-11-23.md) can inform future **asset mods**. A texture viewer, model exporter or FiveM conversion utility is not proof that GTA VI RAGE 9 runtime will accept arbitrarily replaced assets, new skeletons, altered script bytecode or mission skipping.

## Intended modular design, not an implemented mod loader

**Stage A — proven native game boot and render:** exact build identity, import/shader/asset validation, game/renderer ABI correctness, no online modification.

**Stage B — offline render/asset overrides:** a user-controlled package manifest declaring target build, supplied resources, replacements, origin/license, and hashes. A deterministic, reversible mount/overlay order avoids destructive changes to original game files. Separate renderer provider APIs for native/DLSS SR/FSR/XeSS; *optional* neural passes and Frame Generation (default OFF). Prefer stable exposed engine/renderer hooks over blind memory patching.

**Stage C — modded gameplay:** after mapping an actual RAGE 9 script VM/native command ABI, support versioned script APIs, UI/phone extensions, camera/input/tuning, animation/weapon/outfit replacements, mission progression test mode, and explicit permissions. Script mutation and mission skips require savegame backup/isolated profile, checkpoint integrity tests and recovery from unsupported states. NPC/traffic setting changes must preserve scripted actors and mission traffic, as already represented by [1080p60 profile](GTA_VI_1080P60_QUALITY_NO_FG_PROFILE_2026-11-23.md).

**Stage D — compatibility/security:** resource caps on file reads/decompression and mod memory, per-mod disable controls, rollback and crash isolation where possible, signature/manifest auditing for binary extensions, user choice of installation source, no auto-downloading untrusted modules and no anti-cheat workarounds. Offline/single-player mods must not be silently enabled in official online sessions.

**Engineering acceptance:** every claimed setting must provide a verifiable runtime hook, an actual working scene, GPU/CPU time, p95/p99 frametime, visual screenshots and rollback test. The user-requested RTX 4070 1080p/60 real FPS raster profile is a goal; neither neural-rendering mod injection nor a source-language switch guarantees it.

No game assets/executable, NVIDIA DLLs, GPL source import, runtime hook/injection, scheduled jobs, recurring search or background monitor were introduced by this document.