# Vanta Third-Party Dependencies

All vendored third-party dependencies live under `3dparty`.

Each dependency should provide:

- `METADATA` with the upstream version, source, license name, and retrieved files.
- `LICENSE` with the upstream license text.
- `vanta_3p.cmake` defining stable `Vanta3p::*` targets.

Source dependencies should keep upstream files under `include` and `src`.

Binary dependencies should separate artifacts by platform, architecture, and build
configuration:

```text
lib/<platform>/<arch>/<config>/
bin/<platform>/<arch>/<config>/
```

Examples:

```text
lib/macos/arm64/release/
lib/windows/x64/debug/
lib/linux/x86_64/release/
```

Project code should depend on `Vanta3p::*` targets instead of referencing
`3dparty` paths directly.
