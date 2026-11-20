#!/usr/bin/env bash
# The full gate (docs/GUARDRAILS.md -> Sanitizer builds): plain Debug, then
# ASan + UBSan, then TSan. Stops at the first failure. Run from anywhere; it
# works from the repository root.
#
#   plain  build-ninja  every test; also builds tools/miditiming, so a change
#                       that breaks the timing tool fails here and not at the
#                       next timing check (it is not built in the instrumented
#                       configurations: timing is never measured there)
#   asan   build-asan   every test (HUMS_SANITIZE=address)
#   tsan   build-tsan   engine tests and canaries (HUMS_SANITIZE=thread)

set -euo pipefail

cd "$(dirname "$0")/.."

summaries=()

run_configuration() {
    local label="$1" dir="$2" sanitize="$3" filter="$4" extra_target="${5:-}"
    local log="${dir}/gate-ctest.log"

    echo "=== gate: ${label} (${dir}) ==="
    cmake -B "${dir}" -G Ninja -DCMAKE_BUILD_TYPE=Debug -DHUMS_SANITIZE="${sanitize}" > /dev/null
    cmake --build "${dir}" --target hums_tests
    if [[ -n "${extra_target}" ]]; then
        cmake --build "${dir}" --target "${extra_target}"
    fi

    local ctest_args=(--test-dir "${dir}" --output-on-failure --no-tests=error)
    if [[ -n "${filter}" ]]; then
        ctest_args+=(-R "${filter}")
    fi

    if ! ctest "${ctest_args[@]}" 2>&1 | tee "${log}"; then
        echo "=== gate FAILED in ${label} ==="
        exit 1
    fi
    summaries+=("${label}: $(grep -E 'tests passed' "${log}")")
}

run_configuration "plain"       build-ninja ""        ""               miditiming
run_configuration "asan+ubsan"  build-asan  address   ""
run_configuration "tsan"        build-tsan  thread    "engine|canary"

echo
echo "=== gate passed ==="
printf '%s\n' "${summaries[@]}"
