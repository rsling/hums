#!/usr/bin/env python3
"""Guardrail 1: schema names are spelled as strings only in src/model/Ids.h.

docs/GUARDRAILS.md -> 1. The check reads Ids.h, collects the string of every
juce::Identifier declared there, and then looks through the rest of the
source root for one of those strings written as a literal where an identifier
is constructed or a ValueTree property or child is named:

    juce::Identifier ("pitch")        juce::Identifier pitch { "pitch" }
    node.getProperty ("pitch")        node.setProperty ("pitch", 60, nullptr)
    node.hasType ("Note")             node.getChildWithName ("Events")
    juce::ValueTree note { "Note" }   node["pitch"]

and the other calls in IDENTIFIER_CALLS. A hit anywhere inside the call's
brackets counts. The same word in a comment, a log message or a UI label is
not a hit. Reported with file:line and a rule name in square brackets:

  [identifier]  a schema name spelled as a string outside Ids.h
  [literal-name] in model/, io/ and render/ only: any string literal, schema
                name or not, as the name in one of those places: the first
                argument of a call or construction, the initialiser of an
                Identifier, a subscript. In these layers every ValueTree is
                the document, so a name that is not a constant from Ids.h is
                an invented one [D15]. The value in
                setProperty (ids::kind, "event", ...) is not a name and not
                a hit. views/ and app/ keep the first rule only, because
                there the same call names can belong to other things.
  [ids-file]    a declaration in Ids.h that is not of the one fixed form
                    inline const juce::Identifier pitch { "pitch" };
                with the constant named exactly like its string. The check
                can only trust a file it can read completely.

Exit codes: 0 = no violation, 1 = violations, 2 = the check could not run.
Every violation is a line starting "check_identifiers: VIOLATION"; the last
line gives the count. The must-fail fixture tests look for both.

Runs with the Python that ships with the pinned Xcode (TOOLCHAIN.md); standard
library only.
"""

import argparse
import os
import pathlib
import re
import sys

from cpp_text import blank_out_comments

NAME = "check_identifiers"
REPOSITORY_ROOT = pathlib.Path(__file__).resolve().parent.parent

# The allowed exceptions, and only these (docs/GUARDRAILS.md -> 1), relative
# to the repository root. Listed one by one; never a wildcard.
#
# Only src/ is scanned, so the second entry excuses nothing today. It is here
# because the contract names it: if the scan is ever widened to tests/, the
# project fixtures stay exempt. (tests/guardrails/ToolchainJuceTest.cpp spells
# two identifiers of its own as literals, on purpose; they are not schema
# names and would not be hits either.)
EXEMPT_FILES = ("src/io/Migrations.cpp",)
EXEMPT_DIRECTORIES = ("tests/io/fixtures/",)

# The layers where every ValueTree is the document, as directories under the
# source root: the [literal-name] rule applies here.
DOCUMENT_ONLY_LAYERS = ("model/", "io/", "render/")

SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".m", ".mm", ".inl", ".inc", ".ipp"}

# Calls whose arguments name a ValueTree type, property or child.
IDENTIFIER_CALLS = (
    "getProperty",
    "setProperty",
    "hasProperty",
    "removeProperty",
    "getPropertyAsValue",
    "getPropertyPointer",
    "getChildWithName",
    "getOrCreateChildWithName",
    "getChildWithProperty",
    "hasType",
)

# Types constructed from a name: "Type (...)", "Type {...}", and the
# declaration forms "Type variable (...)", "Type variable {...}",
# "Type variable = ...".
IDENTIFIER_TYPES = ("Identifier", "ValueTree")

STRING_LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')

CALL_OPENING = re.compile(r"\b(" + "|".join(IDENTIFIER_CALLS) + r")\s*\(")
CONSTRUCTION_OPENING = re.compile(r"\b(" + "|".join(IDENTIFIER_TYPES) + r")\b\s*(?:\w+\s*)?[({]")
ASSIGNMENT = re.compile(r"\b(" + "|".join(IDENTIFIER_TYPES) + r")\b\s*\w+\s*=\s*" + STRING_LITERAL.pattern)
SUBSCRIPT = re.compile(r"\[\s*" + STRING_LITERAL.pattern + r"\s*\]")

IDS_DECLARATION = re.compile(
    r'^\s*inline\s+const\s+juce::Identifier\s+(\w+)\s*\{\s*"([^"\\]*)"\s*\}\s*;\s*$'
)

CLOSING_BRACKET = {"(": ")", "{": "}"}


class CheckError(Exception):
    """The check could not run (as opposed to: it ran and found a violation)."""


class Violation:
    def __init__(self, file, line_number, rule, message):
        self.file = file  # as it should be printed
        self.line_number = line_number
        self.rule = rule
        self.message = message


def read_source(path):
    try:
        return blank_out_comments(path.read_text(encoding="utf-8", errors="replace"))
    except OSError as error:
        raise CheckError(f"cannot read {path}: {error}")


def line_number_at(text, position):
    return text.count("\n", 0, position) + 1


def display_path(path):
    """A path as it should appear in messages: relative to the repository if inside it."""
    try:
        return path.resolve().relative_to(REPOSITORY_ROOT).as_posix()
    except ValueError:
        return path.as_posix()


def read_ids_file(ids_file):
    """Returns (the set of identifier strings, violations in the file itself)."""
    names = set()
    violations = []
    shown = display_path(ids_file)

    for line_number, line in enumerate(read_source(ids_file).splitlines(), start=1):
        if "Identifier" not in line:
            continue
        declaration = IDS_DECLARATION.match(line)
        if not declaration:
            violations.append(
                Violation(
                    shown,
                    line_number,
                    "ids-file",
                    'not of the form: inline const juce::Identifier pitch { "pitch" };',
                )
            )
            continue
        constant, string = declaration.group(1), declaration.group(2)
        if constant != string:
            violations.append(
                Violation(
                    shown,
                    line_number,
                    "ids-file",
                    f'the constant {constant} holds "{string}"; '
                    f"a constant is named exactly like its string",
                )
            )
        names.add(string)

    if not names:
        raise CheckError(f"{ids_file} declares no identifier; nothing to check against")
    return names, violations


def end_of_brackets(text, opening_position):
    """The position just past the bracket that closes the one at opening_position.

    Brackets inside string literals do not count. An unclosed bracket ends at
    the end of the text.
    """
    depth = 0
    position = opening_position
    length = len(text)
    while position < length:
        character = text[position]
        if character == '"':
            literal = STRING_LITERAL.match(text, position)
            position = literal.end() if literal else position + 1
            continue
        if character in "({":
            depth += 1
        elif character in ")}":
            depth -= 1
            if depth == 0:
                return position + 1
        position += 1
    return length


def end_of_first_argument(text, opening_position):
    """The position of the comma or bracket that ends the first argument."""
    depth = 0
    position = opening_position
    length = len(text)
    while position < length:
        character = text[position]
        if character == '"':
            literal = STRING_LITERAL.match(text, position)
            position = literal.end() if literal else position + 1
            continue
        if character in "({[":
            depth += 1
        elif character in ")}]":
            depth -= 1
            if depth == 0:
                return position
        elif character == "," and depth == 1:
            return position
        position += 1
    return length


def hits_in_file(text, names, document_only_layer):
    """(position, rule, string, where it was found) for every hit, in order.

    A schema name anywhere inside the brackets is an [identifier] hit in every
    layer. In a document-only layer, any other literal in the place of the
    name is a [literal-name] hit.
    """
    hits = {}

    def remember(position, string, where, is_in_name_place):
        if position in hits:
            return
        if string in names:
            hits[position] = ("identifier", string, where)
        elif document_only_layer and is_in_name_place:
            hits[position] = ("literal-name", string, where)

    for opening_pattern in (CALL_OPENING, CONSTRUCTION_OPENING):
        for opening in opening_pattern.finditer(text):
            bracket_position = opening.end() - 1
            end = end_of_brackets(text, bracket_position)
            first_argument_end = end_of_first_argument(text, bracket_position)
            where = f"{opening.group(1)} {text[bracket_position]}...{CLOSING_BRACKET[text[bracket_position]]}"
            for literal in STRING_LITERAL.finditer(text, bracket_position, end):
                remember(literal.start(), literal.group(1), where, literal.start() < first_argument_end)

    for assignment in ASSIGNMENT.finditer(text):
        remember(assignment.start(2) - 1, assignment.group(2), f"{assignment.group(1)} ... = ", True)

    for subscript in SUBSCRIPT.finditer(text):
        remember(subscript.start(1) - 1, subscript.group(1), "[...]", True)

    return [(position, *hits[position]) for position in sorted(hits)]


def is_exempt(path):
    try:
        relative = path.resolve().relative_to(REPOSITORY_ROOT).as_posix()
    except ValueError:
        return False
    return relative in EXEMPT_FILES or relative.startswith(EXEMPT_DIRECTORIES)


def source_files(source_root):
    found = []
    for directory, _, file_names in os.walk(source_root):
        for file_name in file_names:
            path = pathlib.Path(directory) / file_name
            if path.suffix in SOURCE_SUFFIXES:
                found.append(path)
    return sorted(found)


def find_violations(source_root, ids_file):
    if not source_root.is_dir():
        raise CheckError(f"{source_root} is not a directory")

    names, violations = read_ids_file(ids_file)
    ids_file_resolved = ids_file.resolve()

    for path in source_files(source_root):
        if path.resolve() == ids_file_resolved or is_exempt(path):
            continue
        text = read_source(path)
        layer_path = path.relative_to(source_root).as_posix()
        document_only_layer = layer_path.startswith(DOCUMENT_ONLY_LAYERS)
        for position, rule, string, where in hits_in_file(text, names, document_only_layer):
            if rule == "identifier":
                message = (
                    f'"{string}" is spelled as a string in {where}; '
                    f"use model::ids::{string} ({display_path(ids_file)})"
                )
            else:
                message = (
                    f'"{string}" names a type, property or child as a string in {where}; '
                    f"in model/, io/ and render/ every such name is a constant from "
                    f"{display_path(ids_file)}"
                )
            violations.append(
                Violation(display_path(path), line_number_at(text, position), rule, message)
            )
    return violations


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument(
        "--source-root",
        type=pathlib.Path,
        default=REPOSITORY_ROOT / "src",
        help="the directory to scan (a must-fail fixture passes its own)",
    )
    parser.add_argument(
        "--ids-file",
        type=pathlib.Path,
        default=REPOSITORY_ROOT / "src" / "model" / "Ids.h",
        help="the one file that may spell schema names as strings",
    )
    arguments = parser.parse_args()

    try:
        violations = find_violations(arguments.source_root, arguments.ids_file)
    except CheckError as error:
        print(f"{NAME}: ERROR: {error}")
        return 2

    for violation in violations:
        print(
            f"{NAME}: VIOLATION: {violation.file}:{violation.line_number}: "
            f"[{violation.rule}] {violation.message}"
        )
    print(f"{NAME}: {len(violations)} violation(s) in {display_path(arguments.source_root)}")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
