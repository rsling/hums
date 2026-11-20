# The layer libraries (CLAUDE.md -> Toolchain, Layers and dependencies).
#
# Every layer is declared here from the start, with its dependencies, but a
# layer becomes a static library target only once its source list names a
# file: CMake cannot build a library without sources, and a placeholder source
# would be throw-away code (rule 10). Until then configure reports the layer as
# empty. The include graph itself is checked by guardrail 2, not by CMake.
#
#   hums_add_layer(<target>
#                  SOURCES <files...>     may be empty for now; headers only
#                                         makes an INTERFACE library
#                  LAYERS  <targets...>   layers this one includes
#                  [JUCE])                links hums_juce (the pinned JUCE modules)
#
# Sources include each other relative to src/, e.g. "render/TempoMap.h".
function(hums_add_layer target)
    cmake_parse_arguments(PARSE_ARGV 1 layer "JUCE" "" "SOURCES;LAYERS")
    if(layer_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "hums_add_layer(${target}): unknown arguments ${layer_UNPARSED_ARGUMENTS}")
    endif()

    if(NOT layer_SOURCES)
        message(STATUS "Hums layer ${target}: no sources yet")
        return()
    endif()

    foreach(dependency IN LISTS layer_LAYERS)
        if(NOT TARGET ${dependency})
            message(FATAL_ERROR "Hums layer ${target} depends on ${dependency}, which has no sources yet")
        endif()
    endforeach()

    # A layer whose sources are all headers (the platform/ interfaces) has
    # nothing to compile, and CMake can't build a static library from headers.
    # It becomes an INTERFACE library that carries the include path and its
    # dependencies.
    set(compiled_sources ${layer_SOURCES})
    list(FILTER compiled_sources INCLUDE REGEX "\\.(cpp|mm)$")
    if(NOT compiled_sources)
        if(layer_JUCE)
            message(FATAL_ERROR "Hums layer ${target} is header-only and cannot link JUCE")
        endif()
        add_library(${target} INTERFACE ${layer_SOURCES})
        target_include_directories(${target} INTERFACE "${PROJECT_SOURCE_DIR}/src")
        target_link_libraries(${target} INTERFACE ${layer_LAYERS})
        return()
    endif()

    add_library(${target} STATIC ${layer_SOURCES})
    target_include_directories(${target} PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(${target} PUBLIC ${layer_LAYERS})
    if(layer_JUCE)
        target_link_libraries(${target} PUBLIC hums_juce)
    endif()
    # Compiler flags only: links no JUCE code and adds no JUCE include path, so
    # the render core stays JUCE-free.
    target_link_libraries(${target} PRIVATE juce::juce_recommended_warning_flags)
endfunction()

# In dependency order: a layer is added after every layer it includes.
add_subdirectory(platform)
add_subdirectory(platform/mac)
add_subdirectory(model)
add_subdirectory(render)
add_subdirectory(io)
add_subdirectory(engine)
