# Toolchain — Hums

Pinned versions for building and testing Hums. `CLAUDE.md → Toolchain` says *what* is pinned and how; this file records the exact versions. A pin moves only on purpose, after Roland agrees, with the change recorded here in the same commit. Timing numbers are valid only for the versions listed here.

Started 20 Nov 2026 with the versions installed on Roland's Mac. Build-time baselines added at the close of milestone 0 and measured again at the close of milestone 1, and once more on 24 Nov 2026, after the fixes of the milestone 1 code review and before the merge into `main`.

## Pinned

| component | version | notes |
|---|---|---|
| JUCE | **9.0.6** | git submodule `libs/JUCE` at the `9.0.6` release tag (commit `654bdbc7a9db`); also in `libs/JUCE_VERSION`. Pinned for the whole Hums v1 cycle. Newest JUCE release on GitHub when checked on 20 Nov 2026. Requires CMake ≥ 3.22 and a deployment target ≥ macOS 10.11 (`libs/JUCE/CMakeLists.txt`, `juce_TargetPlatform.h`) |
| Xcode | 27.0 (build 27A266a) | `/Applications/Xcode.app` |
| Apple clang | 21.0.0 (clang-2100.3.34.2) | ships with Xcode 27.0; target `arm64-apple-darwin` |
| clang-format | 21.0.0 (clang-2100.3.34.2) | ships with Xcode 27.0; run as `xcrun clang-format`, never a MacPorts or Homebrew one, which formats differently. Style in `.clang-format` (decided 21 Nov 2026) |
| Python | 3.9.6 | ships with Xcode 27.0; runs the guardrail scripts in `tools/` as `xcrun python3`, never a MacPorts or Homebrew one. Standard library only, so the scripts must stay within Python 3.9 |
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
| LeakSanitizer | **not shipped** | `clang++: error: unsupported option '-fsanitize=leak' for target 'arm64-apple-darwin27.0.0'`. Leaks stay with JUCE's leak detector (Debug); its printed report fails the test (`docs/GUARDRAILS.md → Sanitizer builds`) |
| RealtimeSanitizer | **not shipped** | `unsupported option '-fsanitize=realtime'`. The `[[clang::nonblocking]]` attribute and `-Wfunction-effects` (compile-time check) do compile. No RTSan configuration in v1 **[D10]** |

On macOS the sanitizers `abort()` after a report by default; the canary tests set `abort_on_error=0` so CTest judges them by their output.

## Build-time baselines (measured 20 Nov 2026, again 22 and 24 Nov 2026)

Mac Studio, Apple M1 Ultra (20 cores), macOS 27.0.1, with the pins above; `build-ninja` (plain Debug, Ninja); wall-clock time from `/usr/bin/time`. These are the reference for noticing when the build gets slow, not targets.

| item | close of milestone 0 (20 Nov 2026) | close of milestone 1 (22 Nov 2026) | milestone 1 after its code review (24 Nov 2026) |
|---|---|---|---|
| Clean build, Debug, Ninja, ccache off (`CCACHE_DISABLE=1`, after `--target clean`) | **6.7 s** wall (83 s user); 140 steps: `hums_juce`, Catch2, the layer libraries, the test executables, `miditiming` | **14.0 s** wall (196 s user); 265 steps: the same, now with the `model` and `io` layers and their tests | **14.6 s** wall (202 s user); 277 steps |
| Clean build, Debug, Ninja, ccache warm | 1.2 s wall | 3.0 s wall | 2.7 s wall (2.63 and 2.67; a first run, in which not everything came from the cache, took 4.8 s) |
| Incremental `hums_tests` rebuild after touching one layer source, ccache off — **proxy** | **0.74 s** wall (same in three runs). Touched `src/platform/mac/MidiBytesToUmp.cpp`: rebuilds that one object, re-archives `libhums_platform_mac.a`, relinks `platform_tests`; nothing else, `miditiming` included | **0.67 s** wall (0.66 to 0.68 in three runs); the same three steps | **0.77 s** wall (0.77 to 0.82 in three runs); the same three steps |
| The same after touching a `model/` source, ccache off | — (no `model/` source existed) | **1.7 s** wall (1.70 to 1.74 in three runs). Touched `src/model/Validator.cpp`: rebuilds that one object, re-archives `libhums_model.a`, relinks `model_tests`, `io_tests` and `guardrail_properties`; nothing else | **2.0 s** wall (1.95 to 2.01 in three runs); the same five steps |

**Why a proxy.** The milestone 0 criterion names a render-core source and asks that the rebuild neither compile nor link the app. At milestone 0 neither exists: the render core gets its first file at milestone 2, the app target at milestone 3, and a placeholder would be throw-away code (rule 10). The `platform/mac/` source is the only layer source with a test executable, so it stands in. Milestone 2 repeats the measurement on the first render-core source and records it here; milestone 3 rechecks that it still compiles and links no app code (`docs/MILESTONES.md`).

How to repeat (ccache off so the touched file really compiles):

```
cmake --build build-ninja --target clean
/usr/bin/time -p env CCACHE_DISABLE=1 cmake --build build-ninja
touch <source>
/usr/bin/time -p env CCACHE_DISABLE=1 cmake --build build-ninja --target hums_tests -v
```
