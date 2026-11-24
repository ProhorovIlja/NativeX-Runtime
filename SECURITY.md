# Security and provenance

- This public source repository must not contain proprietary binaries, game archives, protected SDK files, extracted proprietary source or shaders, firmware, credentials, keys, personal data or memory dumps.
- Only use inputs you are authorized to access and analyze. No DRM or access-control bypass is required by the public test suite.
- Treat downloaded executable files and external releases as untrusted. Never run arbitrary binaries to inspect metadata; use the bounded offline reader and independently built fixtures.
- Keep private inputs outside the checkout. Review staged files and diffs before publishing; ignore rules are only a defense in depth, not access control.
- Verify provenance, license terms and dependency revisions before adoption. Respect third-party notices.
- Automatic GitHub Actions are disabled by the repository owner. No automatic execution or background monitoring should be introduced. Prefer on-demand local tests.

Use a minimal security report with no confidential data. See [public scope](docs/PUBLIC_SCOPE.md).
