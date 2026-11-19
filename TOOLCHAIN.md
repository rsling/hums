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
| macOS deployment target | 15.0 | `CMAKE_OSX_DEPLOYMENT_TARGET`, set before `project()`; arm64 only. Chosen 20 Nov 2026 to leave room for an older studio Mac |
| Catch2 | v3.16.1 | `FetchContent`, `GIT_TAG v3.16.1`, shallow; newest v3 tag on 20 Nov 2026 |
| ccache | 4.13.1 | MacPorts (`/opt/local/bin/ccache`). Required: `find_program(... REQUIRED)` sets `CMAKE_{C,CXX,OBJC,OBJCXX}_COMPILER_LAUNCHER` |

Development host at the time of pinning: macOS 27.0.1, Apple silicon (arm64).

## Sanitizer support (checked 20 Nov 2026)

| sanitizer | Xcode 27.0 clang | notes |
|---|---|---|
| AddressSanitizer + UBSan | shipped | `HUMS_SANITIZE=address`; canaries `guardrail_canary_asan_use_after_free`, `guardrail_canary_ubsan_signed_overflow` |
| ThreadSanitizer | shipped | `HUMS_SANITIZE=thread`; canary `guardrail_canary_tsan_data_race` |
| LeakSanitizer | **not shipped** | `clang++: error: unsupported option '-fsanitize=leak' for target 'arm64-apple-darwin27.0.0'`. Leaks stay with JUCE's leak detector (Debug) |
| RealtimeSanitizer | **not shipped** | `unsupported option '-fsanitize=realtime'`. The `[[clang::nonblocking]]` attribute and `-Wfunction-effects` (compile-time check) do compile. Bears on `docs/DECISIONS.md` Q7 |

On macOS the sanitizers `abort()` after a report by default; the canary tests set `abort_on_error=0` so CTest judges them by their output.

## To be filled in at milestone 0

| item | value |
|---|---|
| Clean build time, Debug, Ninja | — |
| Incremental `hums_tests` rebuild after touching one render-core source | — |
