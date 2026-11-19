# Sanitizer canaries (docs/GUARDRAILS.md -> Sanitizer builds): each program has
# one planted bug and passes only when its sanitizer's report appears in the
# output. A canary built without its flags prints no report and fails, so a
# configuration that has lost its instrumentation fails the gate. Built only in
# the instrumented configurations, and only the canaries that configuration's
# sanitizer can see.

function(hums_add_canary name source report_regex)
    add_executable(${name} ${source})
    add_dependencies(hums_tests ${name})
    add_test(NAME ${name} COMMAND ${name})
    # On macOS the sanitizers abort() after a report by default, and CTest
    # counts a crash as a failure whatever the output says. abort_on_error=0
    # makes them exit with a non-zero code instead; the regex alone then
    # decides the result.
    set_tests_properties(${name} PROPERTIES
        PASS_REGULAR_EXPRESSION "${report_regex}"
        ENVIRONMENT "ASAN_OPTIONS=abort_on_error=0;UBSAN_OPTIONS=abort_on_error=0;TSAN_OPTIONS=abort_on_error=0")
endfunction()

if(HUMS_SANITIZE STREQUAL "address")
    hums_add_canary(guardrail_canary_asan_use_after_free
        canary_asan_use_after_free.cpp "AddressSanitizer: heap-use-after-free")
    hums_add_canary(guardrail_canary_ubsan_signed_overflow
        canary_ubsan_signed_overflow.cpp "runtime error: signed integer overflow")
elseif(HUMS_SANITIZE STREQUAL "thread")
    hums_add_canary(guardrail_canary_tsan_data_race
        canary_tsan_data_race.cpp "ThreadSanitizer: data race")
endif()
