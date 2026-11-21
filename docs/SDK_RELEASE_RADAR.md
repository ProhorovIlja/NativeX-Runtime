# Official PC technology SDK release radar

`tools/sdk_radar.py` scans the **GitHub API metadata** of a bounded list of six official/community projects in `sources/sdk_releases.json`. Daily source radar also tracks separate Xbox Series runtime commits and forks; this tool adds updates of major external PC SDKs and Linux compatibility technologies.

Watched references:
- AMD FidelityFX SDK: FSR graphics effects.
- Intel XeSS SDK: super resolution, FG and latency APIs.
- NVIDIA Streamline: public integration interfaces for DLSS and Reflex (not a license or permission to ship full DLSS 5 neural runtime).
- vkd3d-proton: Direct3D12 → Vulkan for Linux/Proton Windows-app compatibility; this is **not** a native ELF port.
- Microsoft DirectStorage: Windows storage SDK/sample repository (may not publish standard GitHub releases).
- Valve Proton: Linux execution of Windows software (not a native Linux build).

It only queries `/repos/{owner}/{repo}/releases/latest`. GitHub's `latest` endpoint may omit prerelease tags or newer commits. An HTTP 404 is recorded as `no-release`, not a nonexistent repository or error. Unavailable network/API state is distinguished from no release. The report never infers SDK functionality from a version tag.

Safe offline check:

```sh
python -m unittest discover -s tests -v
python tools/sdk_radar.py --offline-fixture tests/fixtures/sdk_release_metadata.json --report artifacts/sdk-releases.md --snapshot artifacts/sdk-releases.json
```

Scheduled public GitHub CI queries API metadata using read-only built-in GitHub token and retains small reports. It does **not** download package archives, SDK binaries, DLSS models, game files, executables, keys or installer software. Actual capability requires authorized SDK integration and native Windows/Linux renderer/physical GPU tests.

The 2026-11-22 reference snapshot records AMD v2.4.0, XeSS v3.0.2, NVIDIA Streamline v2.14.16, vkd3d-proton v3.1, Proton proton-11.0-2; DirectStorage `latest` returned 404. These are *GitHub release metadata*, not compatibility proof, and will change independently of this repository.
