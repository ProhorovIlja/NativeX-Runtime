# Security and provenance

- Source-code-only public repository. Do **not** upload retail game archives or EXEs, console DLLs, shaders extracted from proprietary games, protected SDK files, firmware, credentials, keys, private account data, crash dumps or copyrighted game media.
- No instructions or code for circumventing retail DRM are required for our synthetic interoperability tests.
- Treat downloaded executables, GitHub releases and untrusted PRs as potentially malicious. CI must not execute arbitrary upstream binaries or extract unverified archives.
- Keep local experimental inputs outside the repository. Never publish entire process-memory snapshots or title-specific storage.
- Never claim a title runs from a README metric or video alone. Record exact SHA and distinguish source review, synthetic test, live hardware test and independently reproduced result.
- Any future dependency must pass license/provenance review; do not mechanically merge unrelated projects or GPL/LGPL code.
- GitHub Actions run only on standard hosted Linux/Windows runners in this **public** repository. No paid runner types, external paid services or self-hosted hardware are required.

For security reports, open a minimal issue without confidential data.