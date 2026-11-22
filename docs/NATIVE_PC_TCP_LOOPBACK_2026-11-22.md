# Native Windows/Linux loopback TCP bounded transport — two child processes

Original C++17 native Win32 Winsock and Linux POSIX sockets executable: `fixtures/native_tcp_loopback.cpp` (2026-11-22). This is a **local platform capability test**, not multiplayer gameplay, NAT traversal, Xbox Live compatibility, matchmaking, co-op session discovery, replication or a title network protocol.

## Contract tested

A parent process opens only a dynamically allocated TCP port on **127.0.0.1**. It never binds to `0.0.0.0`, an external address or a public network. A separate child process connects using genuine operating system sockets (Win32 `WSAStartup/socket/connect/send/recv` and `CreateProcessW`; Linux ELF `socket/connect/send/recv` and `fork`). A deterministic original **256 KiB** payload is sent in deliberate bounded 4096-byte chunks; every byte is verified on the receiver, the receiver sends a different 256 KiB response and the client independently validates it.

Frames are prefixed with a network-order **4-byte length + 8-byte FNV-1a checksum**. Short reads/writes are handled by explicit loops. Each accepted socket gets 5 s send/receive timeouts and the listener uses a bounded `select` readiness check. A **second child process** sends an intentionally oversized (256 KiB + 1) declared frame; the listener rejects the header **before allocating a payload buffer** and closes the local socket. The test checks that this path is reached.

Parent/child resources close automatically when the process completes; the listener has an ephemeral port and exits. No background service, firewall port opening, new workflow, scheduled task or real user data is required. No external SDK or game asset is imported.

## Limits and next acceptance

FNV-1a detects accidental corruption only; it is neither a cryptographic MAC nor authenticated session protection. TCP loopback on one host proves basic OS socket and byte-framing semantics, **not** internet reliability, traffic shaping, encryption, packet loss recovery, P2P NAT punching, low latency, QoS or the correctness of a shared open-world game state.

The user-provided GTA VI development MD explicitly prioritizes **full native Windows single-player compatibility first**, Linux native ELF second, and only then the co-op project. This fixture is reusable system infrastructure, not a change to that development order. Actual game netcode cannot be designed from invented serialization or ownership assumptions before inspecting an authorized title build. Later work must assess Steam Networking Sockets vs native UDP/TCP and consented friend/session authentication as separate, measured product decisions.
