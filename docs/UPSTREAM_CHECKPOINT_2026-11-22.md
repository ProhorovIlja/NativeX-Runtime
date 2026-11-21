# Public GitHub observation checkpoint — 2026-11-22

The public upstream radar originally compared against fixed SHAs from November 21. Since 9 tracked heads had already moved, every subsequent daily report would otherwise flag the same old changes as "new". This checkpoint moves the current comparison baseline **without losing the original baseline**: every repository row retains initial_baseline_sha.

This is a GitHub branch-tip observation, **not a clean-clone build**, game execution, verified fix, trusted release, security assessment or endorsement. The registry still names exact repository/branch and preserves the historical first SHA. Each new report now flags changes relative to this snapshot.

| Repository | First baseline | Observed checkpoint | Change |
|---|---|---|---|
| [omonkeyz/xbox-emulator](https://github.com/omonkeyz/xbox-emulator/tree/main) | `6590adbe2135` | [`0fc72956f9ff`](https://github.com/omonkeyz/xbox-emulator/commit/0fc72956f9ff742a275534c27d9e5a7a9886a0c3) | observed moved |
| [drycheetah84/TomBox](https://github.com/drycheetah84/TomBox/tree/master) | `7e8064fdcd47` | [`7425a91fa8d1`](https://github.com/drycheetah84/TomBox/commit/7425a91fa8d1ee1b9517c833b3183919aab3ea7f) | observed moved |
| [ivall/xcl](https://github.com/ivall/xcl/tree/main) | `3c546ef78e6b` | [`f12d724dd94b`](https://github.com/ivall/xcl/commit/f12d724dd94b6ab99acef04866e3e2f8da7b9e) | observed moved |
| [californiaAlmonds/OpenScarlett](https://github.com/californiaAlmonds/OpenScarlett/tree/main) | `bcc28958f94f` | [`7cc28a7bc5bd`](https://github.com/californiaAlmonds/OpenScarlett/commit/7cc28a7bc5bdd69cd475b70a1064287eee6e560c) | observed moved |
| [Ender-0001/GDKScarlett](https://github.com/Ender-0001/GDKScarlett/tree/main) | `e703bc3a11ad` | [`e703bc3a11ad`](https://github.com/Ender-0001/GDKScarlett/commit/e703bc3a11ad2780c9167f98c25b9867c7b08797) | unchanged |
| [Freminet/ScarlettX](https://github.com/Freminet/ScarlettX/tree/main) | `bda18b4d3dab` | [`bda18b4d3dab`](https://github.com/Freminet/ScarlettX/commit/bda18b4d3dab501dd0f372e5d1921f462c6bd457) | unchanged |
| [R0Y00/Xbox2Windows](https://github.com/R0Y00/Xbox2Windows/tree/main) | `496fc28aafe3` | [`6ee2406ea69a`](https://github.com/R0Y00/Xbox2Windows/commit/6ee2406ea69a26e47c846033ff4cb8c1ac9b27c0) | observed moved |
| [ItsNotRexed/Leonida](https://github.com/ItsNotRexed/Leonida/tree/main) | `4c75ce537add` | [`36ace7135e89`](https://github.com/ItsNotRexed/Leonida/commit/36ace7135e897c228eede5d1d8c28e972285cb14) | observed moved |
| [realdody/americas-recomp](https://github.com/realdody/americas-recomp/tree/xbox-series) | `e7833ef44668` | [`3a0c38b3ae10`](https://github.com/realdody/americas-recomp/commit/3a0c38b3ae10135288b96a582d1214e8a79ccbef) | observed moved |
| [oguzhanozfe/gta-vi-port-research](https://github.com/oguzhanozfe/gta-vi-port-research/tree/main) | `df423f4f45fc` | [`df423f4f45fc`](https://github.com/oguzhanozfe/gta-vi-port-research/commit/df423f4f45fc397a98b872cab9db9f1aee2df8c5) | unchanged |
| [brcly/TuxBox](https://github.com/brcly/TuxBox/tree/main) | `6d15471b0eba` | [`d5abfc01dd55`](https://github.com/brcly/TuxBox/commit/d5abfc01dd551aae4841be7de5038ba78185d2b4) | observed moved |
| [andrewnakas/gtav-recomp](https://github.com/andrewnakas/gtav-recomp/tree/main) | `2a76184f563e` | [`2a76184f563e`](https://github.com/andrewnakas/gtav-recomp/commit/2a76184f563e4b604a88bea8eeed1334e77f3220) | unchanged |
| [microsoft/GDK](https://github.com/microsoft/GDK/tree/Main) | `d773744074fb` | [`d773744074fb`](https://github.com/microsoft/GDK/commit/d773744074fb757ef27b2356c9aef5497b14623b) | unchanged |
| [LooperSalty/AnyXboxX](https://github.com/LooperSalty/AnyXboxX/tree/main) | `1aad95584d8b` | [`c9a243270ceb`](https://github.com/LooperSalty/AnyXboxX/commit/c9a243270ceb309c15fe82b0b6b763ff29061d9e) | observed moved |

## Source review highlights, distinct from correctness evidence

- TomBox at 7425a91 merged RPF package scanning and PSO-aware shader/resource translation work (upstream source only), not a verified GTA VI shader pipeline.
- OpenScarlett at 7cc28a7 addressed an MSVC/runtime DLL crash compatibility problem, and reported a synthetic CTest suite. Those upstream results were not independently replayed here.
- Xbox2Windows at 6ee2406 advanced shader/effect inventory and scalar-ALU lowering; coverage is not correctness.
- AnyXboxX at c9a2432 added further RDNA2 instruction operand/disassembly coverage, not live rendering proof.
- XCL at f12d724 continues command-list and native GDK research; downstream adoption requires ABI, Windows version, license and resource validation.
- The core project now has a source-built synthetic graphics/DMA command stream test pending in PR #22. It is NOT a decoded Xbox record format.

Use tools/radar.py and tools/commit_watch.py for bounded public observation. A returned unavailable status remains unknown, not unchanged. No game executable, assets, keys, third-party source checkout or downloaded binaries are included.
