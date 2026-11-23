# Native read-only asset overlay — 2026-11-23

## Why this stage is possible without a game executable

The proposed user goal is future GTA VI native Windows/Linux modding with replaceable textures, models, outfits, UI, game settings, and eventually script/mission extensions. Existing public RAGE 9 projects have made meaningful progress extracting archives, textures and models from **reportedly accessible Xbox game resources**, without a runnable GTA VI executable. That is a good reason to build asset-mod infrastructure **now**, but not to claim that this source-built component can mount GTA VI RPF9/BCPack or inject assets into the RAGE 9 runtime.

[Leonida](https://github.com/ItsNotRexed/Leonida), read directly at 2026-11-23 HEAD [561b65a8](https://github.com/ItsNotRexed/Leonida/commit/561b65a832218fbb9e6d2effd392a8eef093a3f8), explicitly says the title **does not boot**; it reports shader translation/validation and a synthetic host/test executable, plus a model extraction tally, GPU resource and draw-path progress, but limitations in BCPack/history-dependent blocks and untested actual-game render execution. This project's clean-room, original **data-file overlay** is independent of Leonida's GPL-3.0 implementation. No code or assets were imported.

## Actual original C++17 implementation

[Header](../src/native_asset_overlay.hpp) and [implementation](../src/native_asset_overlay.cpp) provide `scarlett::overlay::Catalog(original_root, override_root)`, a bounded, synchronous **read-only resolution table**.

- Virtual asset name maps to the real original file unless it has a registered valid override; **last registered wins**. Clearing registrations restores the original path without deleting or modifying any original bytes.
- Paths are normalized for **ASCII case-insensitive virtual name matching** and slash/backslash equivalence. For this early safe proof of concept, all file-path segments are **ASCII only**, <=80 bytes each, and <=240 total bytes. Later Unicode paths require a separate defined normalization and collision policy.
- Rejects absolute paths, drive letters, UNC prefixes, empty and dot/parent segments, invalid punctuation, leading/trailing spaces and trailing dots, overlong input, and Windows reserved devices (CON/NUL/AUX/PRN/COM1–9/LPT1–9 with suffixes). No implicit interpretation of `..` aliases.
- Rejects file replacement with executable/plugin/script launcher suffixes: EXE, DLL, ASI, SO, DYLIB, BAT, CMD, PS1, COM, SCR. This is a **data-only overlay prototype**, not a binary/script loader.
- Uses filesystem symlink checks on **all asset subpath components**, verifies regular-file type, canonicalizes root and candidate and checks actual path containment. Rejects symlink targets rather than accepting their canonical destination. These checks occur both **on registration and on resolution**. A deleted/changed override fails visibly instead of silently substituting an old game original.
- Limits registered overrides to **512** entries to bound its purpose as a testable first milestone. This is not a claim that 512 is sufficient for real game mod packs.
- Returns `Status`, `Origin` and an **existing canonical file path**, ready to pass to the existing [portable source-built asynchronous asset Reader](../src/native_asset_stream.hpp). It never copies, edits, executes, patches, writes or automatically downloads the original game or mod files. It never starts services or monitors.
- Precedence is deterministic for the explicitly registered assets. Registration and resolution aren't thread-safe; the future engine/mount integration must supply proper synchronization and a clear loading phase.

### Security boundaries

This is a **planning-time file-path validator**. It is NOT a race-free security sandbox against an actively malicious local process swapping paths between check and open (**TOCTOU**). Production untrusted mod loading needs platform-specific file-handle-relative resolution (e.g., Linux openat2 RESOLVE_BENEATH and RESOLVE_NO_SYMLINKS, and equivalent Windows HANDLE-based checks), content signatures/hash-based manifests, size/decompression and memory quotas, atomic mount snapshots, and sandboxing of any native code. For now the underlying Reader accepts a path and performs a normal file open.

Important: checking a file suffix does not establish safe **file contents**. All RPF9/BCPack/TTPF/mesh/audio/script file formats still require their own strict size bounds, version checks and parsers. No console-specific format is accepted or claimed here.

For Linux, virtual key matching ignores ASCII case, but original filesystem lookup still uses the caller's case-preserved relative path; original-case alias resolution in a case-sensitive root is not implemented. A future canonical asset index with strict collision handling is required.

## Synthetic Windows/Linux acceptance

The [native_asset_overlay_tests](../fixtures/native_asset_overlay_tests.cpp) executable has five groups:

1. Original file fallback; two independently registered overrides; case/Windows separator matching; last-wins precedence; exact bytes read through the **existing real portable asynchronous Reader**; originals unchanged; clear and restore.
2. Rejection of absolute, UNC, drive, traversal, empty/dot, DOS reserved device, overlong and malformed paths.
3. Rejection of executables/plugin file targets, missing payload, missing original and directory-as-file.
4. Override disappears after registration: fail closed; if symlink creation is supported on the host, reject an attempted symlink substitution to another root (Windows with no symlink privilege may skip just that branch).
5. Bounded 512-entry registration and source-path traversal rejection.

This tests **synthetic files created locally during tests**, not Xbox game assets, model replaceability, physical GPU rendering or end-to-end GTA VI execution.

## Roadmap once real format/engine data is verifiable

First identify RAGE 9 virtual asset keys, build/version names, pack precedence and resource lifecycles from independent verifiable research. Next integrate a bounded RPF9 decoder and BCPack/TTPF pipeline with output validation, then provide **manifest-based user-controlled mount overlays** tied to original base hashes. Only after actual title loader and rendering are verified should an on-disk override be treated as an applied texture/outfit/model change.

For script and mission-skip mods, a separate version-checked **game VM/natives API** and save/checkpoint consistency framework will be needed: asset file substitution by itself does not change runtime game logic or safely complete mission state.

The goal is **native Windows x64 first**, Linux separately as native ELF where technically possible; Windows PE under Proton is a different Linux compatibility path. No CPU emulation introduced, no new scheduled tasks, ongoing monitoring, background downloads or agents.
