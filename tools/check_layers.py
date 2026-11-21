#!/usr/bin/env python3
"""Guardrail 2: the include graph under src/ obeys the layer rules.

docs/GUARDRAILS.md -> 2, and the table under CLAUDE.md -> Layers and
dependencies. The check reads every #include / #import line under the source
root and reports, with file:line and a rule name in square brackets:

  [layer]            a file includes a project header its layer may not see
                     (the table below), including the std-only render core,
                     the std-only platform/ interfaces, and platform/mac/
                     being visible to app/ only
  [juce]             JUCE in the render core or in a platform/ interface
  [os-header]        an angle-bracket include that is neither std nor JUCE
                     (CoreMIDI, mach, Cocoa, pthread, any third-party
                     library) outside platform/mac/
  [objc-file]        a .m / .mm file outside platform/mac/
  [facade]           app/TransportFacade.h reaches an engine/ header, directly
                     or through other headers
  [unknown-layer]    a source file that belongs to no layer
  [unresolved]       a quoted include that names no file under the source
                     root with exactly that spelling; such an include cannot
                     be assigned to a layer, so it is refused
  [computed-include] #include with a macro instead of a file name

Quoted includes are resolved the way the compiler does it here: relative to
the including file first, then relative to the source root. File names are
compared case-sensitively although the macOS file system is not. Every include
line counts, also one inside #if 0: the check does not evaluate the
preprocessor.

Exit codes: 0 = no violation, 1 = violations, 2 = the check could not run.
Every violation is a line starting "check_layers: VIOLATION"; the last line
gives the count. The must-fail fixture tests look for both.

Runs with the Python that ships with the pinned Xcode (TOOLCHAIN.md); standard
library only.
"""

import argparse
import os
import pathlib
import posixpath
import re
import sys

NAME = "check_layers"
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent

# ---------------------------------------------------------------------------
# The layers. A file's layer follows from its path under the source root.

PLATFORM = "platform/ (interfaces)"
PLATFORM_MAC = "platform/mac/"
MODEL = "model/"
IO = "io/"
RENDER_CORE = "the render core"
RENDER = "render/ (outside the core)"
ENGINE = "engine/"
VIEWS = "views/"
APP = "app/"

# The render core: render/snapshot/ and these files directly in render/,
# headers and sources alike (CLAUDE.md -> Repository layout).
RENDER_CORE_DIRECTORY = "render/snapshot/"
RENDER_CORE_FILE_STEMS = {"TempoMap", "Kernel", "NoteTracker"}

# The one app/ header views/ may include, and the header whose transitive
# includes must stay clear of engine/.
TRANSPORT_FACADE = "app/TransportFacade.h"

# Which layers' headers a layer may include. This is the table in CLAUDE.md;
# change both together, and only on purpose.
ALLOWED_LAYERS = {
    PLATFORM: {PLATFORM},
    PLATFORM_MAC: {PLATFORM, PLATFORM_MAC},
    MODEL: {MODEL},
    IO: {IO, MODEL, RENDER, RENDER_CORE},
    RENDER_CORE: {RENDER_CORE},
    RENDER: {RENDER, RENDER_CORE, MODEL},
    ENGINE: {ENGINE, RENDER_CORE, PLATFORM},
    VIEWS: {VIEWS, MODEL},  # plus TRANSPORT_FACADE, handled where it is checked
    APP: {PLATFORM, PLATFORM_MAC, MODEL, IO, RENDER_CORE, RENDER, ENGINE, VIEWS, APP},
}

# Layers that must not include JUCE: they are std-only by contract.
STD_ONLY_LAYERS = {PLATFORM, RENDER_CORE}

# The only layer where OS headers and Objective-C(++) files may appear.
OS_LAYER = PLATFORM_MAC

SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".m", ".mm", ".inl", ".inc", ".ipp"}
OBJC_SUFFIXES = {".m", ".mm"}

# Named in docs/GUARDRAILS.md; used only to word the message. Every other
# non-std, non-JUCE angle include is refused just the same.
OS_HEADER_PREFIXES = (
    "CoreMIDI/",
    "CoreFoundation/",
    "CoreAudio/",
    "mach/",
    "Cocoa/",
    "AppKit/",
    "Foundation/",
    "objc/",
)

# The C++20 standard library headers. <stdint.h>-style C names are left out on
# purpose: C++ code here uses the <c...> forms.
STD_HEADERS = set(
    """
    algorithm any array atomic barrier bit bitset charconv chrono codecvt
    compare complex concepts condition_variable coroutine deque exception
    execution filesystem format forward_list fstream functional future
    initializer_list iomanip ios iosfwd iostream istream iterator latch limits
    list locale map memory memory_resource mutex new numbers numeric optional
    ostream queue random ranges ratio regex scoped_allocator semaphore set
    shared_mutex source_location span sstream stack stdexcept stop_token
    streambuf string string_view strstream syncstream system_error thread
    tuple type_traits typeindex typeinfo unordered_map unordered_set utility
    valarray variant vector version
    cassert cctype cerrno cfenv cfloat cinttypes climits clocale cmath csetjmp
    csignal cstdarg cstddef cstdint cstdio cstdlib cstring ctime cuchar cwchar
    cwctype
    """.split()
)

INCLUDE_DIRECTIVE = re.compile(r"^\s*#\s*(?:include|import|include_next)\b\s*(.*)$")
QUOTED_TARGET = re.compile(r'^"([^"]*)"')
ANGLE_TARGET = re.compile(r"^<([^>]*)>")


class CheckError(Exception):
    """The check could not run (as opposed to: it ran and found a violation)."""


class Include:
    def __init__(self, line_number, is_quoted, target):
        self.line_number = line_number
        self.is_quoted = is_quoted  # False: angle brackets
        self.target = target  # None: a computed include


class Violation:
    def __init__(self, file, line_number, rule, message):
        self.file = file
        self.line_number = line_number
        self.rule = rule
        self.message = message


def layer_of(relative_path):
    """The layer of a file, from its path relative to the source root, or None."""
    if relative_path.startswith("platform/mac/"):
        return PLATFORM_MAC
    if relative_path.startswith("platform/"):
        return PLATFORM
    if relative_path.startswith("model/"):
        return MODEL
    if relative_path.startswith("io/"):
        return IO
    if relative_path.startswith(RENDER_CORE_DIRECTORY):
        return RENDER_CORE
    if relative_path.startswith("render/"):
        directly_in_render = "/" not in relative_path[len("render/") :]
        stem = posixpath.splitext(posixpath.basename(relative_path))[0]
        if directly_in_render and stem in RENDER_CORE_FILE_STEMS:
            return RENDER_CORE
        return RENDER
    if relative_path.startswith("engine/"):
        return ENGINE
    if relative_path.startswith("views/"):
        return VIEWS
    if relative_path.startswith("app/"):
        return APP
    return None


def blank_out_comments(text):
    """Replaces comment text with spaces; keeps line breaks and string literals.

    An include line inside a comment is not an include, and a // inside a
    string literal is not a comment.
    """
    result = []
    index = 0
    length = len(text)
    while index < length:
        character = text[index]
        following = text[index + 1] if index + 1 < length else ""
        if character == "/" and following == "/":
            end = text.find("\n", index)
            end = length if end == -1 else end
            result.append(" " * (end - index))
            index = end
        elif character == "/" and following == "*":
            end = text.find("*/", index + 2)
            end = length if end == -1 else end + 2
            result.append("".join("\n" if c == "\n" else " " for c in text[index:end]))
            index = end
        elif character in "\"'":
            end = index + 1
            while end < length and text[end] != character and text[end] != "\n":
                end += 2 if text[end] == "\\" else 1
            end = min(end + 1, length)
            result.append(text[index:end])
            index = end
        else:
            result.append(character)
            index += 1
    return "".join(result)


def read_includes(path):
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        raise CheckError(f"cannot read {path}: {error}")

    includes = []
    for line_number, line in enumerate(blank_out_comments(text).splitlines(), start=1):
        directive = INCLUDE_DIRECTIVE.match(line)
        if not directive:
            continue
        rest = directive.group(1)
        quoted = QUOTED_TARGET.match(rest)
        angle = ANGLE_TARGET.match(rest)
        if quoted:
            includes.append(Include(line_number, True, quoted.group(1)))
        elif angle:
            includes.append(Include(line_number, False, angle.group(1)))
        else:
            includes.append(Include(line_number, False, None))
    return includes


def collect_files(source_root):
    """Every file under the source root, as a POSIX path relative to it."""
    files = set()
    for directory, _, names in os.walk(source_root):
        for name in names:
            absolute = pathlib.Path(directory) / name
            files.add(absolute.relative_to(source_root).as_posix())
    return files


def resolve_quoted(including_file, target, all_files):
    """The file a quoted include names, relative to the source root, or None.

    Looked up in the set of files found on disk, so the spelling must match
    exactly, and a path that leaves the source root never resolves.
    """
    beside_including_file = posixpath.normpath(
        posixpath.join(posixpath.dirname(including_file), target)
    )
    from_source_root = posixpath.normpath(target)
    for candidate in (beside_including_file, from_source_root):
        if candidate in all_files:
            return candidate
    return None


def is_std_header(target):
    return target in STD_HEADERS


def is_juce_header(target):
    return target.startswith("juce_")


def check_angle_include(file, layer, include):
    if is_std_header(include.target):
        return []
    if is_juce_header(include.target):
        if layer in STD_ONLY_LAYERS:
            return [
                Violation(
                    file,
                    include.line_number,
                    "juce",
                    f"{layer} is std-only and includes JUCE <{include.target}>",
                )
            ]
        return []
    if layer == OS_LAYER:
        return []
    if include.target.startswith(OS_HEADER_PREFIXES):
        what = f"OS header <{include.target}>"
    else:
        what = f"<{include.target}>, which is neither std nor JUCE"
    return [
        Violation(
            file,
            include.line_number,
            "os-header",
            f"{layer} includes {what}; only platform/mac/ may",
        )
    ]


def check_quoted_include(file, layer, include, all_files):
    """Returns (violations, resolved path or None)."""
    resolved = resolve_quoted(file, include.target, all_files)
    if resolved is None:
        message = (
            f'"{include.target}" names no file under the source root with exactly '
            f"that spelling; std, JUCE and OS headers are included with <...>"
        )
        return [Violation(file, include.line_number, "unresolved", message)], None

    included_layer = layer_of(resolved)
    if included_layer is None:
        # The included file is reported on its own as [unknown-layer].
        return [], resolved

    allowed = included_layer in ALLOWED_LAYERS[layer]
    if layer == VIEWS and resolved == TRANSPORT_FACADE:
        allowed = True
    if allowed:
        return [], resolved

    if layer == VIEWS and included_layer == APP:
        reason = f"views/ may include only {TRANSPORT_FACADE} from app/"
    else:
        reason = f"{layer} may not include {included_layer}"
    return [
        Violation(file, include.line_number, "layer", f'"{include.target}": {reason}')
    ], resolved


def check_transport_facade(project_includes):
    """app/TransportFacade.h must not reach an engine/ header, however indirectly.

    project_includes maps a file to its resolved quoted includes as
    (line number, included file) pairs.
    """
    if TRANSPORT_FACADE not in project_includes:
        return []

    violations = []
    chain_to = {TRANSPORT_FACADE: [TRANSPORT_FACADE]}
    pending = [TRANSPORT_FACADE]
    while pending:
        current = pending.pop(0)
        for line_number, included in project_includes.get(current, []):
            if included in chain_to:
                continue
            chain_to[included] = chain_to[current] + [included]
            if layer_of(included) == ENGINE:
                chain = " -> ".join(chain_to[included])
                violations.append(
                    Violation(
                        current,
                        line_number,
                        "facade",
                        f"{TRANSPORT_FACADE} must not reach an engine/ header: {chain}",
                    )
                )
                continue
            pending.append(included)
    return violations


def find_violations(source_root):
    if not source_root.is_dir():
        raise CheckError(f"{source_root} is not a directory")

    all_files = collect_files(source_root)
    violations = []
    project_includes = {}

    for file in sorted(all_files):
        suffix = posixpath.splitext(file)[1]
        if suffix not in SOURCE_SUFFIXES:
            continue

        layer = layer_of(file)
        if layer is None:
            violations.append(
                Violation(file, 1, "unknown-layer", "this file belongs to no layer")
            )
            continue

        if suffix in OBJC_SUFFIXES and layer != OS_LAYER:
            violations.append(
                Violation(
                    file,
                    1,
                    "objc-file",
                    f"Objective-C(++) file in {layer}; only platform/mac/ may hold one",
                )
            )

        resolved_includes = []
        for include in read_includes(source_root / file):
            if include.target is None:
                violations.append(
                    Violation(
                        file,
                        include.line_number,
                        "computed-include",
                        "include without a literal file name cannot be checked",
                    )
                )
            elif include.is_quoted:
                found, resolved = check_quoted_include(file, layer, include, all_files)
                violations.extend(found)
                if resolved is not None:
                    resolved_includes.append((include.line_number, resolved))
            else:
                violations.extend(check_angle_include(file, layer, include))
        project_includes[file] = resolved_includes

    violations.extend(check_transport_facade(project_includes))
    return violations


def display_path(source_root):
    """The source root as it should appear in messages: relative to the repository if inside it."""
    try:
        return source_root.resolve().relative_to(REPOSITORY_ROOT).as_posix()
    except ValueError:
        return source_root.as_posix()


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--source-root",
        type=pathlib.Path,
        default=REPOSITORY_ROOT / "src",
        help="the directory holding the layer directories (a must-fail fixture passes its own)",
    )
    arguments = parser.parse_args()

    try:
        violations = find_violations(arguments.source_root)
    except CheckError as error:
        print(f"{NAME}: ERROR: {error}")
        return 2

    prefix = display_path(arguments.source_root)
    for violation in violations:
        print(
            f"{NAME}: VIOLATION: {prefix}/{violation.file}:{violation.line_number}: "
            f"[{violation.rule}] {violation.message}"
        )
    print(f"{NAME}: {len(violations)} violation(s) in {prefix}")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
