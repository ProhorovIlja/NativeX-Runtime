# GTA VI Windows native first — deferred private co-op and mod scope (2026-11-23)

## Binding priority: port before gameplay additions

This document preserves user-requested features for **later**. It is **NOT an implemented networking subsystem, verified RAGE 9 interface or decision to divert engineering work away from booting the game**.

**P0 — Native Windows x64 executable integration and single-player correctness.** Obtain an authorized, independently verified readable build; correctly load game code; match the Xbox GDK/GameCore import ABI, memory mappings, GPU resources, command submission, shaders, frame presentation, audio, input, asset streaming, filesystem and saves. Prove menu -> world -> missions -> save/reload on real hardware. Existing synthetic C++17 fixtures, shader translations and archive research are preparatory and cannot demonstrate a playable GTA VI port.

**P1 — Native PC quality and efficiency.** Genuine mouse/keyboard with configurable scan-code binds, UI input, full gamepad and analog features, audio, frame pacing, low-stutter disk/VRAM management, display modes and scaling, support for multiple hardware tiers. Native rendered 1080p/60 FPS on Ryzen 5 5600 + RTX 4070 12 GB + 32 GB RAM **remains a goal**, not a guaranteed result; FG OFF, optional native/DLSS SR Quality/Balanced, RT off only when a working non-RT path is verified. Evaluate GPU and CPU bottlenecks from real title frames, not guessed minimum specs. Fix correctness and crashes before optional feature expansion.

**P2 — Bounded offline extensibility.** Document actual RAGE 9 asset paths, loader behavior and script VM/natives ABI; implement reversible user-controlled asset overrides, versioned manifests, isolated saves and optional mod APIs only after game format validation. Support mod disable/rollback, no auto-downloading untrusted packages, no proprietary bundled game data, and respect source/provider licenses.

**P3 — Private two-player story co-op once single-player is reliable.** No claims of multiplayer working until two real clients can connect, join the same world, drive, enter missions, disconnect/rejoin and recover a consistent save.

**P4 — Optional gameplay/UI changes**, gated by confirmed engine access and proper per-feature testing.

## Deferred two-player character design

- Baseline playable roles: one Jason, one Lucia. Allow player/role selection, establish ownership, camera, mission roles, inventory, vehicles and animation context independently. Consider role switching only if script control, story state and checkpoints allow it.
- Optional **appearance replacement**, not a guaranteed new playable character system: another suitable male model for the Jason role or female model for the Lucia role, with the original role's dialogue/voice retained by default. Names mentioned by the user (e.g. Mandy) are ideas **not verified as present playable GTA VI assets**. Do not assume compatibility just from model gender; skeletons, facial rigs, morph targets, hair, clothing slots, collision, cutscenes, mocap and animation retargeting must be checked separately. Fail back to the original character rather than corrupting scenes.
- No compulsory AI voice transformation. Synthesizing someone else's in-game voice is not a prerequisite to cosmetic swaps, and could have significant compute, licensing and identity concerns.
- Future contextual animation/emote wheel may reuse **verified usable** existing assets outside cutscenes when state machines and skeletons support it; joint animations require both players' explicit consent, correct placement/IK and safe interruption. Cutscene files are not automatically suitable for dynamic gameplay.

## Deferred private-co-op networking engineering

Architecture candidates must be selected **after** examining the RAGE 9 timing/state model and CPU budgets. A practical starting hypothesis is authenticated encrypted direct P2P via ICE/STUN and an optional relay fallback for restrictive NAT/CGNAT. **P2P is a connectivity mode, not a consistency algorithm**. Do not promise guaranteed direct paths, zero latency or unlimited host capacity; allow cancellation/retries, avoid busy polling, bound packets and queues, and test under packet loss, jitter and disconnection.

For authoritative world state, prefer a clearly designated **session/mission host** over two independently authoritative peers, unless actual engine behavior proves a different arrangement correct. Explicitly define ownership and versioned replication for players, movement, vehicles, physics, pedestrians, AI, wanted level, weapons, inventory, pickups, mission triggers, shared world events, time/weather, interiors, cutscenes and persistent progression. Evaluate selective prediction/interpolation and interest management, but do not use client prediction to invent persistent mission outcomes. Host migration, reconnect, resynchronization, drop-in/drop-out and checkpoint recovery need deliberately tested rules. Do not assume replay/rollback of a complete open world is cheap or deterministic.

Keep **private modded story co-op logically and operationally separate** from any official multiplayer service or accounts. Offline/private cheats must never silently propagate into official sessions; agree on host/session permissions and prevent data corruption or unauthorized item/stat writes.

## Deferred in-game additions

- **Phone-to-phone chat:** send messages from the player's in-game phone to the other player's device (Jason ↔ Lucia), with real keyboard typing, UTF-8, emoji and potentially optional externally sourced GIF/media. Needs a verified phone UI API, network authentication, bounded message size/rate, safe rendering, media controls, and stable UI/game input focus. Does not require changes to the original character voices.
- **Completion dashboard:** accurate 100% progress visibility by mission, collectibles, activities and milestones, only when internal game progression flags and counting rules are verified.
- **Mission skip / auto-complete:** only for user-selected uninteresting content, after mapping associated checkpoint transitions, mission prerequisites, unlocks, inventory, XP/stats, money, relationships, NPC/vehicle world changes, achievements and saved state. Atomic change and explicit isolated save backup/rollback. Never treat 'one completed flag' as sufficient.
- **Optional private-session cheats:** infinite health/stamina/money, configurable stat floors/maxima, and unlock-all for items/outfits/weapons where the game supports these states. Explicit local/private consent and save separation; effects may be disabled for synced story missions when they break authoritative consistency.
- **Optional purchases/persistence:** additional houses, cars/garage slots, and quality-of-life inventory, only after confirming what is already part of RAGE 9's legitimate save model. Prevent orphaned entities and broken save/load.
- **Optional appearance/animation improvements:** existing models, outfits, hair, vehicle/camera/cutscene animation reuse subject to runtime rig/material/collision compatibility, content permissions and graphics budgets.

## Deferred acceptance gates (not implemented features)

1. Single-player real build loads, reliably renders, accepts native controls, works offline and survives repeated save/reload.
2. Real-profile performance on user/reference hardware measured with actual simulation/rendered frames, p95/p99 frametime, GPU pass metrics, RAM/VRAM and low-stutter streaming. No frame-generation count inflation.
3. Real-game versioned asset/animation/scripting hooks validated with originals-preserving mod isolation.
4. Two-client private authenticated join, Jason/Lucia world state, vehicles, a full shared mission, disconnect/reconnect and consistent checkpoint/save tested under packet loss and NAT/relay variations.
5. Only then optional character appearance, phone messaging, completion/skip/cheats, emotes and other quality-of-life packages.

Project-wide invariant: first build a **correct, fast, stable native game**, not a theoretical all-features layer; later modules must be independently switchable, maintainable and benchmarked. There is no working GTA VI playable port or tested co-op from this document.

No automation, scheduled check, recurring job, background monitor, service, notifications or untrusted download was created.
