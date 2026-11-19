# Toolchain — Hums

Pinned versions for building and testing Hums. `CLAUDE.md → Toolchain` says *what* is pinned and how; this file records the exact versions. A pin moves only on purpose, after Roland agrees, with the change recorded here in the same commit. Timing numbers are valid only for the versions listed here.

Started 20 Nov 2026 with the versions installed on Roland's Mac. Completed at milestone 0.

## Pinned

| component | version | notes |
|---|---|---|
| JUCE | **9.0.6** | git submodule `libs/JUCE` at the `9.0.6` release tag (commit `654bdbc7a9db`); also in `libs/JUCE_VERSION`. Pinned for the whole Hums v1 cycle. Newest JUCE release on GitHub when checked on 20 Nov 2026. Requires CMake ≥ 3.22 and a deployment target ≥ macOS 10.11 (`libs/JUCE/CMakeLists.txt`, `juce_TargetPlatform.h`) |
| Xcode | 27.0 (build 27A266a) | `/Applications/Xcode.app` |
| Apple clang | 21.0.0 (clang-2100.3.34.2) | ships with Xcode 27.0; target `arm64-apple-darwin` |
| macOS SDK | 27.0 | ships with Xcode 27.0 |
| CMake | 3.31.12 | |
| Ninja | 1.13.2 | for the CLI and sanitizer build directories |
| C++ standard | C++20 | |

Development host at the time of pinning: macOS 27.0.1, Apple silicon (arm64).

## To be filled in at milestone 0

| item | value |
|---|---|
| macOS deployment target | — |
| Catch2 v3 tag (FetchContent) | — |
| ccache version and wiring (`CMAKE_<LANG>_COMPILER_LAUNCHER`) | — (not installed on the host yet) |
| LeakSanitizer on Xcode 27.0 clang | — (expected: not shipped) |
| RealtimeSanitizer on Xcode 27.0 clang | — (decides `docs/DECISIONS.md` Q7) |
| Clean build time, Debug, Ninja | — |
| Incremental `hums_tests` rebuild after touching one render-core source | — |
