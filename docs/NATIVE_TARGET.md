# Native host target

The public engineering target is a normal Windows x64 or Linux x64 host application built from authorized source and redistributable dependencies, using native graphics, memory, I/O, audio, input and lifecycle APIs. For compatible x64 code, avoid unnecessary CPU instruction interpretation.

Interoperability experiments using independently sourced or generated binaries are intermediate research. A metadata reader, compatibility adapter, native-loaded fixture or graphics test does not demonstrate a complete executable integration.

Acceptance gates:
1. Reproducible original source build and deterministic unit/fixture tests.
2. Correct host ABI and lifecycle behavior (imports, TLS, exceptions, memory).
3. Correct native graphics, streaming, input, audio and persistence on supported platforms.
4. Differential verification on independent fixtures, including capability and failure paths.
5. Optional application-specific experiments only on lawfully accessible inputs, with separate authorization and evidence.

The repository does not offer a playable commercial title, universal console loader, DRM bypass or redistribution of proprietary software. For public contribution boundaries see [PUBLIC_SCOPE.md](PUBLIC_SCOPE.md).
