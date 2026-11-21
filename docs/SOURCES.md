# Pinned source review (2026-11-21 baseline)

The registry `sources/upstreams.json` records the original GitHub repositories, branches, exact SHAs, author descriptions and GitHub license metadata. This project contains **none** of their source trees or proprietary game data.

Key groups: native Windows HLE and GDK shims (TomBox/omonkeyz, XCL, OpenScarlett, GDKScarlett, ScarlettX), standalone analysis and graphics fixtures (Xbox2Windows, americas-recomp), independent compatibility design (TuxBox, AnyXboxX), Microsoft public GDK documentation and historical recompilation methodology (GTA V/Xbox 360 reference, not Xbox Series ABI).

## Known reported gaps

1. [omonkeyz/xbox-emulator issue #31](https://github.com/omonkeyz/xbox-emulator/issues/31): external report that a clean clone cannot build certain components due to uncommitted `backend_data` files. Needs independently reproduced, source-only clean-build check before adoption.
2. [TomBox issue #5](https://github.com/drycheetah84/TomBox/issues/5): different counting methods report different totals for shader programs; percentages need explicit denominator/uniqueness definitions.
3. [Leonida issue #94](https://github.com/ItsNotRexed/Leonida/issues/94): synthetic wave64 correctness issues with 32-lane subgroups on NVIDIA. Compilable SPIR-V does not imply semantic equivalence.
4. [Xbox2Windows issue #145](https://github.com/R0Y00/Xbox2Windows/issues/145): synthetic wave64 shuffle discrepancy with a particular Windows WARP build; isolate driver/runtime differences before concluding correctness.
5. [AnyXboxX decoder comparison](https://github.com/LooperSalty/AnyXboxX/blob/main/docs/recherche/tombox-decodeur.md): LLVM gfx10/gfx1030 test corpus identified missing/mismatched mnemonic labels in a pinned revision of a decoder. This is **not** proof that those instructions occur in any particular game.

## Next independent comparisons

Native PE loader + import forwarding, synthetic ABI discrepancies, descriptor/root signature semantics, wave32/wave64 cross-subgroup transforms, placed textures, GPU memory aliasing and verified clean-source builds.

All above are public **author/reporter findings**, not independently reproduced in this repository.