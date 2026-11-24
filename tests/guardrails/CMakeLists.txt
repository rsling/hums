# Toolchain smoke tests: prove that every configuration builds real code, with
# and without JUCE, before any layer has sources, and that it runs wherever the
# gate runs every test (plain and ASan + UBSan). The TSan leg of the gate
# builds them too but runs only engine tests and the canary (tools/gate.sh).

# std only, no JUCE: the shape of every future render-core test.
hums_add_test_executable(guardrail_toolchain_std ToolchainStdTest.cpp)
hums_discover_tests(guardrail_toolchain_std "guardrail.toolchain_std.")

# The pinned JUCE, built in this configuration (instrumented or not) through
# hums_juce: the same library the layers link.
hums_add_test_executable(guardrail_toolchain_juce ToolchainJuceTest.cpp)
target_link_libraries(guardrail_toolchain_juce PRIVATE
    hums_juce
    juce::juce_recommended_warning_flags)
hums_discover_tests(guardrail_toolchain_juce "guardrail.toolchain_juce.")

# Guardrail 3: invariants as property tests (docs/GUARDRAILS.md -> 3). The
# generator of random valid documents, the checks every document is held to,
# the driver that applies random commands, the checks every command is held
# to, and the run over the seeds. It saves and loads through the real load path,
# so it links the io layer and the model layer under it.
hums_add_test_executable(guardrail_properties
    CommandChecks.cpp
    CommandChecks.h
    CommandChecksTest.cpp
    CommandDriver.cpp
    CommandDriver.h
    CommandDriverTest.cpp
    DocumentChecks.cpp
    DocumentChecks.h
    DocumentChecksTest.cpp
    DocumentGenerator.cpp
    DocumentGenerator.h
    DocumentGeneratorTest.cpp
    PropertyTest.cpp
    RandomSource.cpp
    RandomSource.h
    RandomSourceTest.cpp
    SeedRange.cpp
    SeedRange.h
    SeedRangeTest.cpp)
target_link_libraries(guardrail_properties PRIVATE
    hums_io
    juce::juce_recommended_warning_flags)
# How many seeds a run goes through when HUMS_PROPERTY_SEEDS does not say
# (SeedRange.h): few enough for the loop between two edits. Fewer still in the
# instrumented configurations, where every seed costs several times as much.
# The 10,000 seeds of milestone 1 are asked for with the variable.
if(HUMS_SANITIZE)
    set(hums_property_seeds_default 200)
else()
    set(hums_property_seeds_default 1000)
endif()
target_compile_definitions(guardrail_properties PRIVATE
    HUMS_PROPERTY_SEEDS_DEFAULT=${hums_property_seeds_default})
hums_discover_tests(guardrail_properties "guardrail.properties.")

# The negative tests for HUMS_JUCE_REPORT_REGEX (tests/CMakeLists.txt): one
# program with a failed jassert and one with a leaked object. Each exits 0 and
# is expected to fail, so it passes only while JUCE's printed report still
# fails a test. Debug only: jassert and the leak detector are compiled out of
# a Release build.
function(hums_add_juce_report_test name source)
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE
        hums_juce
        juce::juce_recommended_warning_flags)
    add_dependencies(hums_tests ${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES
        FAIL_REGULAR_EXPRESSION "${HUMS_JUCE_REPORT_REGEX}"
        WILL_FAIL TRUE)
endfunction()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    hums_add_juce_report_test(guardrail_juce_report_assertion JuceReportAssertion.cpp)
    hums_add_juce_report_test(guardrail_juce_report_leak JuceReportLeak.cpp)
endif()

if(HUMS_SANITIZE)
    add_subdirectory(canaries)
endif()

# ---------------------------------------------------------------------------
# Guardrail scripts (docs/GUARDRAILS.md). Each script in tools/ is registered
# twice: once against the repository, where it must find nothing, and once
# against its fixture in tools/fixtures/, where it must report a violation.
#
# The scripts run with the Python that ships with the pinned Xcode
# (TOOLCHAIN.md), through xcrun like clang-format, so a MacPorts or Homebrew
# Python on the PATH plays no role.
find_program(HUMS_XCRUN xcrun REQUIRED)

# The script must exit 0.
function(hums_add_script_guardrail name script)
    add_test(NAME ${name}
        COMMAND "${HUMS_XCRUN}" python3 "${PROJECT_SOURCE_DIR}/tools/${script}" ${ARGN})
endfunction()

# The script must print its violation line. Judged by that line and not by the
# exit code (WILL_FAIL), because a script that crashes, or a missing Python,
# also exits non-zero and would pass for a detected violation.
function(hums_add_script_guardrail_fixture name script violation_regex)
    add_test(NAME ${name}
        COMMAND "${HUMS_XCRUN}" python3 "${PROJECT_SOURCE_DIR}/tools/${script}" ${ARGN})
    set_tests_properties(${name} PROPERTIES
        PASS_REGULAR_EXPRESSION "${violation_regex}")
endfunction()

# JUCE pin: libs/JUCE is at the tag libs/JUCE_VERSION names. The fixture is a
# version file naming another tag.
hums_add_script_guardrail(guardrail_juce_pin check_juce_pin.py)
hums_add_script_guardrail_fixture(guardrail_juce_pin_fixture_fails check_juce_pin.py
    "check_juce_pin: VIOLATION: .*pins JUCE '9\\.0\\.5'"
    --version-file "${PROJECT_SOURCE_DIR}/tools/fixtures/check_juce_pin/JUCE_VERSION")

# Guardrail 2: the include graph under src/ obeys the layer rules. Each
# fixture is a small tree standing in for src/ with one planted violation; its
# test passes only when the script reports that file and rule and counts
# exactly one violation, so a fixture cannot pass for another reason than its
# own.
hums_add_script_guardrail(guardrail_layers check_layers.py)

function(hums_add_layers_fixture case file_and_line rule)
    hums_add_script_guardrail_fixture(guardrail_layers_fixture_${case}_fails check_layers.py
        "check_layers: VIOLATION: [^\n]*/${case}/${file_and_line}: \\[${rule}\\][^\n]*\ncheck_layers: 1 violation"
        --source-root "${PROJECT_SOURCE_DIR}/tools/fixtures/check_layers/${case}")
endfunction()

# The layer table.
hums_add_layers_fixture(views_includes_engine               "views/pianoroll/PianoRollView.h:5" "layer")
hums_add_layers_fixture(views_includes_other_app_header     "views/common/Knob.h:5"             "layer")
hums_add_layers_fixture(engine_includes_model               "engine/Transport.h:5"              "layer")
hums_add_layers_fixture(engine_includes_render_outside_core "engine/Scheduler.h:5"              "layer")
hums_add_layers_fixture(engine_includes_platform_mac        "engine/Clock.h:5"                  "layer")
hums_add_layers_fixture(model_includes_io                   "model/ClipModel.h:5"               "layer")
# The std-only render core and platform/ interfaces.
hums_add_layers_fixture(render_core_includes_model          "render/snapshot/Snapshot.h:5"      "layer")
hums_add_layers_fixture(render_core_includes_render         "render/Kernel.cpp:3"               "layer")
hums_add_layers_fixture(render_core_includes_juce           "render/TempoMap.h:5"               "juce")
hums_add_layers_fixture(platform_interface_includes_juce    "platform/MidiScheduler.h:5"        "juce")
hums_add_layers_fixture(platform_interface_includes_backend "platform/MonotonicClock.h:5"       "layer")
# OS headers and Objective-C++ only under platform/mac/.
hums_add_layers_fixture(os_header_outside_platform_mac          "engine/Clock.cpp:3"    "os-header")
hums_add_layers_fixture(os_header_in_app                        "app/Main.cpp:3"        "os-header")
hums_add_layers_fixture(third_party_header_outside_platform_mac "model/Validator.cpp:3" "os-header")
hums_add_layers_fixture(objc_file_outside_platform_mac          "app/MenuBar.mm:1"      "objc-file")
# A project header in angle brackets, which platform/mac/ would otherwise take
# for an OS header: refused in every layer, in any spelling of case, and so is
# an angle include with ".." in it.
hums_add_layers_fixture(angle_include_in_platform_mac  "platform/mac/CoreMidiScheduler.h:7"  "angle-include")
hums_add_layers_fixture(angle_include_of_allowed_layer "views/pianoroll/PianoRollView.h:6"   "angle-include")
hums_add_layers_fixture(angle_include_in_other_case    "platform/mac/MachTime.cpp:3"         "angle-include")
hums_add_layers_fixture(angle_include_with_dot_dot     "platform/mac/DeviceIdentifier.cpp:5" "angle-include")
# app/TransportFacade.h reaches no engine/ header.
hums_add_layers_fixture(transport_facade_reaches_engine     "app/TransportTypes.h:3"            "facade")
# Nothing escapes classification.
hums_add_layers_fixture(unknown_layer                       "util/Helper.h:1"                   "unknown-layer")
hums_add_layers_fixture(unresolved_include                  "model/ClipModel.h:5"               "unresolved")
hums_add_layers_fixture(include_leaves_source_root          "model/ClipModel.h:5"               "unresolved")
hums_add_layers_fixture(computed_include                    "model/ClipModel.h:6"               "computed-include")
# A file the check would not read: a header with a suffix that is not on the
# list, which could include anything, and an Objective-C++ file spelled .MM.
hums_add_layers_fixture(unknown_suffix                      "engine/Detail.hxx:1"               "unknown-suffix")
hums_add_layers_fixture(unknown_suffix_in_other_case        "app/MenuBar.MM:1"                  "unknown-suffix")

# Guardrail 1: schema names are spelled as strings only in src/model/Ids.h.
# The first group of fixtures is scanned against the real Ids.h, one fixture
# per way of naming a type, property or child; the second group stands in for
# Ids.h itself. As with guardrail 2, a fixture's test passes only when the
# script reports that file and rule and counts exactly one violation.
hums_add_script_guardrail(guardrail_identifiers check_identifiers.py)

function(hums_add_identifiers_fixture case file_and_line rule)
    set(fixture "${PROJECT_SOURCE_DIR}/tools/fixtures/check_identifiers/${case}")
    set(arguments --source-root "${fixture}")
    if(case MATCHES "^ids_")
        list(APPEND arguments --ids-file "${fixture}/model/Ids.h")
    endif()
    hums_add_script_guardrail_fixture(guardrail_identifiers_fixture_${case}_fails check_identifiers.py
        "check_identifiers: VIOLATION: [^\n]*/${case}/${file_and_line}: \\[${rule}\\][^\n]*\ncheck_identifiers: 1 violation"
        ${arguments})
endfunction()

hums_add_identifiers_fixture(get_property            "views/pianoroll/NoteComponent.cpp:5"   "identifier")
hums_add_identifiers_fixture(set_property            "model/commands/NoteCommands.cpp:6"     "identifier")
hums_add_identifiers_fixture(has_type                "render/SnapshotBuilder.cpp:4"          "identifier")
hums_add_identifiers_fixture(child_with_name         "io/ProjectFile.cpp:4"                  "identifier")
hums_add_identifiers_fixture(subscript               "views/stepseq/CellComponent.cpp:4"     "identifier")
# A subscript opens with a bracket that is code: the brackets in
# "[" + position + "]" are text, and what stands between them is no name.
hums_add_identifiers_fixture(subscript_in_string     "model/Validator.cpp:12"                "identifier")
# The ' in 1'000 opens no character literal (tools/cpp_text.py): a comment
# after such a number is still a comment, and code after it is still code.
hums_add_identifiers_fixture(digit_separator         "model/GridModel.cpp:11"                "identifier")
# A bracket or a comma in a character literal is a character, not code: '}'
# ends no body, '{' lets none run on into the next function (whose "Clip" is
# text), and ',' separates no arguments.
hums_add_identifiers_fixture(character_literal_closing_brace "io/ProjectFile.cpp:9" "identifier")
hums_add_identifiers_fixture(character_literal_opening_brace "views/Label.cpp:11"   "identifier")
hums_add_identifiers_fixture(character_literal_comma         "model/NoteModel.cpp:6" "identifier")
hums_add_identifiers_fixture(identifier_construction "model/ClipModel.cpp:3"                 "identifier")
hums_add_identifiers_fixture(identifier_assignment   "render/SnapshotBuilder.cpp:3"          "identifier")
hums_add_identifiers_fixture(value_tree_construction "model/commands/TrackCommands.cpp:4"    "identifier")
# A value is not a name, also when it equals one: only the first argument of
# a call, and in a ValueTree constructed with properties and subtrees the
# type and the first half of each pair.
hums_add_identifiers_fixture(call_value               "model/commands/EventCommands.cpp:11"   "identifier")
hums_add_identifiers_fixture(value_tree_property_list "model/commands/EventCommands.cpp:9"    "identifier")
hums_add_identifiers_fixture(value_tree_subtree       "io/ProjectFile.cpp:8"                  "identifier")
# The brace after a trailing return type opens the body of a lambda, not the
# brackets of a construction: a value in that body is still no name.
hums_add_identifiers_fixture(trailing_return_value_tree "model/commands/EventCommands.cpp:8"  "identifier")
hums_add_identifiers_fixture(trailing_return_identifier "model/EventModel.cpp:8"              "identifier")

# In model/, io/ and render/ a name that is no schema name at all fails too [D15].
hums_add_identifiers_fixture(literal_name_set_property            "model/commands/NoteCommands.cpp:6" "literal-name")
hums_add_identifiers_fixture(literal_name_child_with_name         "render/SnapshotBuilder.cpp:5"      "literal-name")
hums_add_identifiers_fixture(literal_name_identifier_construction "io/ProjectFile.cpp:3"              "literal-name")
hums_add_identifiers_fixture(literal_name_value_tree_property_list "render/SnapshotBuilder.cpp:6"     "literal-name")

# The other places a juce::Identifier is expected, which converts from a
# string literal without a word: a parameter of a function of ours (the check
# reads the declaration inside the fixture), what a function returns, a list.
hums_add_identifiers_fixture(own_function                          "model/NoteModel.cpp:6"         "identifier")
hums_add_identifiers_fixture(own_function_first_parameter          "model/Settings.cpp:7"          "identifier")
hums_add_identifiers_fixture(own_lambda                            "render/SnapshotBuilder.cpp:6"  "identifier")
hums_add_identifiers_fixture(identifier_return                     "io/ProjectFile.cpp:5"          "identifier")
hums_add_identifiers_fixture(value_tree_return                     "io/ProjectFile.cpp:9"          "identifier")
hums_add_identifiers_fixture(value_tree_return_lambda              "render/SnapshotBuilder.cpp:5"  "identifier")
hums_add_identifiers_fixture(identifier_list                       "model/Validator.cpp:3"         "identifier")
hums_add_identifiers_fixture(identifier_array                      "render/SnapshotBuilder.cpp:3"  "identifier")
hums_add_identifiers_fixture(literal_name_own_function             "model/CellModel.cpp:6"         "literal-name")
hums_add_identifiers_fixture(literal_name_identifier_return_lambda "model/LaneModel.cpp:5"         "literal-name")
# A static member defined outside its class has a qualified name
# (NoteModel::pitch), and a call to a function template of ours can have
# template arguments before its bracket: neither hides the name.
hums_add_identifiers_fixture(identifier_construction_qualified     "model/ClipModel.cpp:5"         "identifier")
hums_add_identifiers_fixture(identifier_assignment_qualified       "render/SnapshotBuilder.cpp:3"  "identifier")
hums_add_identifiers_fixture(identifier_list_qualified             "model/Validator.cpp:3"         "identifier")
hums_add_identifiers_fixture(identifier_array_qualified            "render/SnapshotBuilder.cpp:3"  "identifier")
hums_add_identifiers_fixture(own_function_template_arguments       "model/NoteModel.cpp:8"         "identifier")
hums_add_identifiers_fixture(own_constructor_qualified             "model/NoteModel.cpp:4"         "identifier")
# A member of a struct or class of ours that is a juce::Identifier (the check
# reads the struct inside the fixture): the struct filled from a list in
# braces, alone, in a list, as one more element of a list and as what a
# function returns; the member initialised by a constructor, assigned, and
# named in the list. A value beside the name is no hit here either.
hums_add_identifiers_fixture(struct_member                         "io/ProjectFile.cpp:12"             "identifier")
hums_add_identifiers_fixture(struct_member_list                    "io/ProjectFile.cpp:15"             "identifier")
hums_add_identifiers_fixture(struct_member_array                   "model/Validator.cpp:12"            "identifier")
hums_add_identifiers_fixture(struct_member_push_back               "model/commands/SlotCommands.cpp:7"  "identifier")
hums_add_identifiers_fixture(struct_member_emplace_back            "model/commands/SlotCommands.cpp:14" "identifier")
hums_add_identifiers_fixture(struct_member_return                  "model/commands/SlotCommands.cpp:13" "identifier")
hums_add_identifiers_fixture(struct_member_list_return             "model/commands/SlotCommands.cpp:13" "identifier")
hums_add_identifiers_fixture(struct_member_initialiser             "model/RowKey.cpp:5"                "identifier")
hums_add_identifiers_fixture(struct_member_initialiser_braces      "model/RowKey.cpp:7"                "identifier")
hums_add_identifiers_fixture(struct_member_assignment              "model/RowKey.cpp:13"               "identifier")
hums_add_identifiers_fixture(struct_member_designated              "model/RowKey.cpp:10"               "identifier")
hums_add_identifiers_fixture(literal_name_struct_member            "io/ProjectFile.cpp:13"             "literal-name")
# XML tag and attribute names, in model/, io/ and render/.
hums_add_identifiers_fixture(xml_tag_name                          "io/ProjectFile.cpp:6"          "identifier")
hums_add_identifiers_fixture(xml_construction                      "io/ProjectFile.cpp:5"          "identifier")
hums_add_identifiers_fixture(literal_name_xml_attribute            "io/ProjectFile.cpp:6"          "literal-name")

hums_add_identifiers_fixture(ids_constant_differs_from_string "model/Ids.h:9"  "ids-file")
hums_add_identifiers_fixture(ids_unreadable_declaration       "model/Ids.h:10" "ids-file")
