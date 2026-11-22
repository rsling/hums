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

and the other calls in IDENTIFIER_CALLS. The name is the first argument of
every one of these calls, wherever it stands and on whichever line; what
follows it is a value, and a value is not a name even when it equals one:
setProperty (ids::kind, "cc", ...) is no hit. In a ValueTree constructed with
its properties and subtrees, the names are the type and the first half of
each pair: juce::ValueTree { "Event", { { "kind", "cc" } } } has two. The
body of a lambda after "-> juce::ValueTree {" is no construction. The same
word in a comment, a log message or a UI label is not a hit.

juce::Identifier converts from a string literal without a word, so a literal
is accepted wherever an Identifier is expected, not only in JUCE's own calls.
The check therefore also looks at four more places, and there only at the
literal that stands for the name:

    PropertyReader::getInt (node, "pitch", 60)    a function of ours with a
                                                  juce::Identifier parameter
    juce::Identifier f() { return "pitch"; }      what a function returns as an
    juce::ValueTree g() { return { "Note", {} }; }  Identifier or as a ValueTree
    std::array<juce::Identifier, 2> { ... }       a list of Identifiers
    xml.getIntAttribute ("pitch")                 an XML tag or attribute name

The functions of ours are not a list kept by hand: the check reads every
declaration under the source root and remembers which parameters of which
function are a juce::Identifier, as it reads the names from Ids.h. A call is
matched by the function's name alone, whichever class it belongs to.

Reported with file:line and a rule name in square brackets:

  [identifier]  a schema name spelled as a string outside Ids.h
  [literal-name] in model/, io/ and render/ only: any string literal, schema
                name or not, as the name in one of those places: the first
                argument of a call, the names of a construction, the
                initialiser of an Identifier, a subscript, and the four
                further places above.
                In these layers every ValueTree is the document, so a name
                that is not a constant from Ids.h is an invented one [D15].
                The value in setProperty (ids::kind, "event", ...) is not a
                name and not a hit. views/ and app/ keep the first rule
                only, because there the same call names can belong to other
                things; for the same reason XML names are looked at in
                model/, io/ and render/ only.
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

from cpp_text import blank_out_comments, literal_spans

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

# What stands before the type in a trailing return type: "[] () -> juce::ValueTree {"
# opens the body of a lambda, not the brackets of a construction.
TRAILING_RETURN_ARROW = re.compile(r"->\s*(?:const\s+)?(?:juce\s*::\s*)?$")

IDS_DECLARATION = re.compile(
    r'^\s*inline\s+const\s+juce::Identifier\s+(\w+)\s*\{\s*"([^"\\]*)"\s*\}\s*;\s*$'
)

CLOSING_BRACKET = {"(": ")", "{": "}"}

# --- The other places a juce::Identifier is expected ------------------------
#
# 1. A parameter of one of our own functions. One parameter of a declaration
#    is a juce::Identifier when it reads "const juce::Identifier& property",
#    with or without const, juce::, the reference, the name and a default.
IDENTIFIER_PARAMETER = re.compile(
    r"\s*(?:const\s+)?(?:juce\s*::\s*)?Identifier\s*(?:const\s*)?&{0,2}\s*\w*\s*(?:=.*)?", re.DOTALL
)
FUNCTION_OPENING = re.compile(r"\b(\w+)\s*\(")
LAMBDA_OPENING = re.compile(r"\b(\w+)\s*=\s*\[[^\]]*\]\s*\(")

# Words that stand before a "(" without being the name of a function of ours:
# "if (juce::Identifier name = ...)" declares no function called "if", and
# std::function<void (const juce::Identifier&)> none called "void".
NOT_A_FUNCTION_NAME = frozenset(
    (
        "if", "for", "while", "switch", "return", "catch", "sizeof", "alignof", "decltype",
        "noexcept", "requires", "operator", "void", "bool", "char", "short", "int", "long",
        "signed", "unsigned", "float", "double", "auto", "const",
    )
)

# 2. What a function returns: "juce::Identifier nameOf() { return "tick"; }",
#    a list of them ("std::vector<juce::Identifier> names() { return { ... }; }"),
#    and a lambda with "-> juce::Identifier {".
RETURNS_IDENTIFIER = re.compile(
    r"\bIdentifier\b\s*(?:,[^<>;(){}]*)?>?\s*&?\s*(?:\w+\s*::\s*)*\w+\s*\("
)
TRAILING_RETURN_OF_IDENTIFIER = re.compile(
    r"->\s*(?:const\s+)?(?:juce\s*::\s*)?Identifier\b\s*&?\s*\{"
)
BETWEEN_PARAMETERS_AND_BODY = re.compile(r"\s*(?:(?:const|noexcept|override|final)\s*)*\{")
RETURN_STATEMENT = re.compile(r"\breturn\b([^;]*);")

#    The same for a ValueTree: "juce::ValueTree f() { return { "Event", { ... } }; }"
#    constructs one from the list in braces, because the constructor that
#    takes a type and its properties is not explicit (juce_ValueTree.h at the
#    pinned tag). A variable, "juce::ValueTree event (ids::Event);", has the
#    same opening and no body after its brackets.
RETURNS_VALUE_TREE = re.compile(r"\bValueTree\b\s*&?\s*(?:\w+\s*::\s*)*\w+\s*\(")
TRAILING_RETURN_OF_VALUE_TREE = re.compile(
    r"->\s*(?:const\s+)?(?:juce\s*::\s*)?ValueTree\b\s*&?\s*\{"
)
RETURN_OF_LIST = re.compile(r"\breturn\s*\{")

# 3. A list of Identifiers: "std::array<juce::Identifier, 2> names { ... }",
#    "std::vector<juce::Identifier> names = { ... }", "juce::Identifier names[] = { ... }".
IDENTIFIER_LIST = re.compile(
    r"\bIdentifier\b\s*(?:,[^<>;(){}]*)?>\s*(?:const\s+)?\w*\s*(?:=\s*)?[({]"
    r"|\bIdentifier\b\s+\w+\s*\[[^\]]*\]\s*(?:=\s*)?\{"
)

# 4. juce::XmlElement names a tag or an attribute in the first argument of
#    these calls and of its constructors, mostly as text (a StringRef), so no
#    Identifier is even involved. The list is the one in
#    libs/JUCE/modules/juce_core/xml/juce_XmlElement.h at the pinned tag.
XML_NAME_CALLS = (
    "hasTagName",
    "hasTagNameIgnoringNamespace",
    "setTagName",
    "hasAttribute",
    "getStringAttribute",
    "compareAttribute",
    "getIntAttribute",
    "getDoubleAttribute",
    "getBoolAttribute",
    "setAttribute",
    "removeAttribute",
    "getNextElementWithTagName",
    "getChildByName",
    "getChildByAttribute",
    "createNewChildElement",
    "deleteAllChildElementsWithTagName",
    "getChildElementAllSubText",
    "getChildWithTagNameIterator",
)
XML_TYPE = "XmlElement"
XML_OPENING = re.compile(
    r"\b(" + "|".join(XML_NAME_CALLS) + r")\s*\("
    r"|\b(" + XML_TYPE + r")\b\s*>?\s*(?:\w+\s*)?[({]"
)


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


def arguments_of(text, opening_position, angles_are_brackets):
    """(start, end) of every argument inside the bracket at opening_position.

    In a declaration a "<" opens template arguments, and a comma inside them
    separates nothing: std::map<int, int> is one parameter. In a call a "<"
    is as likely a comparison, so there it is left alone.
    """
    arguments = []
    depth = 0
    angle_depth = 0
    start = opening_position + 1
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
                arguments.append((start, position))
                return arguments
        elif angles_are_brackets and character == "<":
            angle_depth += 1
        elif angles_are_brackets and character == ">" and angle_depth > 0:
            angle_depth -= 1
        elif character == "," and depth == 1 and angle_depth == 0:
            arguments.append((start, position))
            start = position + 1
        position += 1
    arguments.append((start, length))
    return arguments


def bracketed_elements(text, start, end):
    """The position of the opening bracket of every element of the list in braces at [start, end).

    For "{ { ids::kind, value }, NamedValue (ids::a, 1), other }" these are
    the brace of the first element and the round bracket of the second; the
    third has none. Nothing if what stands there is not a list in braces.
    """
    list_position = start + len(text[start:end]) - len(text[start:end].lstrip())
    if list_position >= end or text[list_position] != "{":
        return []
    positions = []
    for element_start, element_end in arguments_of(text, list_position, angles_are_brackets=False):
        opening = re.compile(r"[({]").search(text, element_start, element_end)
        if opening:
            positions.append(opening.start())
    return positions


def name_taking_functions(texts):
    """{function name: the positions of its juce::Identifier parameters}.

    Read from every declaration and definition in the given sources, lambdas
    held in a variable included. Only the name is kept, not the class: two
    functions of one name are told apart by nothing here, and a call to
    either is checked at the positions of both.
    """
    found = {}
    for text in texts:
        for opening_pattern in (FUNCTION_OPENING, LAMBDA_OPENING):
            for opening in opening_pattern.finditer(text):
                name = opening.group(1)
                if name in NOT_A_FUNCTION_NAME or name in IDENTIFIER_TYPES:
                    continue
                parameters = arguments_of(text, opening.end() - 1, angles_are_brackets=True)
                for index, (start, end) in enumerate(parameters):
                    if IDENTIFIER_PARAMETER.fullmatch(text, start, end):
                        found.setdefault(name, set()).add(index)
    return found


def hits_in_file(text, names, document_only_layer, name_functions):
    """(position, rule, string, where it was found) for every hit, in order.

    A schema name in the place of the name is an [identifier] hit in every
    layer: the first argument of one of JUCE's calls, the names of a
    construction, and the further places. In a document-only layer, any
    other literal in the place of the name is a [literal-name] hit.
    """
    hits = {}

    def remember(position, string, where):
        if position in hits:
            return
        if string in names:
            hits[position] = ("identifier", string, where)
        elif document_only_layer:
            hits[position] = ("literal-name", string, where)

    def remember_names(start, end, where):
        for literal in STRING_LITERAL.finditer(text, start, end):
            remember(literal.start(), literal.group(1), where)

    def remember_tree_names(bracket_position, where):
        """The names in the brackets of a ValueTree construction.

        (type, { { property, value }, ... }, { subtree, ... }): the type, the
        first half of every pair in the list of properties, and the same
        again for every subtree written as a list in braces. A value is not
        a name.
        """
        arguments = arguments_of(text, bracket_position, angles_are_brackets=False)
        remember_names(*arguments[0], where)
        if len(arguments) > 1:
            for pair_position in bracketed_elements(text, *arguments[1]):
                pair = arguments_of(text, pair_position, angles_are_brackets=False)
                remember_names(*pair[0], where)
        if len(arguments) > 2:
            for subtree_position in bracketed_elements(text, *arguments[2]):
                if text[subtree_position] == "{":
                    remember_tree_names(subtree_position, where)

    # JUCE's own calls: the name is the first argument of every one of them
    # (juce_ValueTree.h at the pinned tag). What follows it is a value, and
    # a value that equals a schema name, as in setProperty (ids::kind, "cc",
    # ...), is not a name.
    for opening in CALL_OPENING.finditer(text):
        bracket_position = opening.end() - 1
        start, end = arguments_of(text, bracket_position, angles_are_brackets=False)[0]
        remember_names(start, end, f"{opening.group(1)} (...)")

    for opening in CONSTRUCTION_OPENING.finditer(text):
        # "-> juce::ValueTree {" is the return type of a lambda and the brace
        # opens its body. What the body returns is looked at under 2. below.
        if TRAILING_RETURN_ARROW.search(text, max(0, opening.start() - 80), opening.start()):
            continue
        bracket_position = opening.end() - 1
        where = f"{opening.group(1)} {text[bracket_position]}...{CLOSING_BRACKET[text[bracket_position]]}"
        if opening.group(1) == "ValueTree":
            remember_tree_names(bracket_position, where)
        else:
            remember_names(bracket_position, end_of_brackets(text, bracket_position), where)

    for assignment in ASSIGNMENT.finditer(text):
        remember(assignment.start(2) - 1, assignment.group(2), f"{assignment.group(1)} ... = ")

    # A subscript opens with a bracket that is code. In "[" << position << "]"
    # the brackets are text, and what stands between them is no name.
    literals = literal_spans(text)
    for subscript in SUBSCRIPT.finditer(text):
        bracket_position = subscript.start()
        if any(start <= bracket_position < end for start, end in literals):
            continue
        remember(subscript.start(1) - 1, subscript.group(1), "[...]")

    # 1. Calls to our own functions: only the arguments that are an Identifier.
    #    The optional word is the variable in "PropertyName name (...)", for a
    #    constructor that takes one.
    if name_functions:
        own_call_opening = re.compile(
            r"\b(" + "|".join(sorted(name_functions)) + r")\b\s*(?:\w+\s*)?[({]"
        )
        for opening in own_call_opening.finditer(text):
            arguments = arguments_of(text, opening.end() - 1, angles_are_brackets=False)
            for index in sorted(name_functions[opening.group(1)]):
                if index < len(arguments):
                    start, end = arguments[index]
                    remember_names(start, end, f"{opening.group(1)} (...)")

    # 2. Every return statement in the body of a function that returns an
    #    Identifier or a list of them.
    body_positions = []
    for opening in RETURNS_IDENTIFIER.finditer(text):
        end_of_parameters = end_of_brackets(text, opening.end() - 1)
        body = BETWEEN_PARAMETERS_AND_BODY.match(text, end_of_parameters)
        if body:
            body_positions.append(body.end() - 1)
    for opening in TRAILING_RETURN_OF_IDENTIFIER.finditer(text):
        body_positions.append(opening.end() - 1)
    for body_position in body_positions:
        end_of_body = end_of_brackets(text, body_position)
        for statement in RETURN_STATEMENT.finditer(text, body_position, end_of_body):
            remember_names(statement.start(1), statement.end(1), "the return of an Identifier")

    #    And every "return { ... }" in the body of a function that returns a
    #    ValueTree: the list in braces is a ValueTree construction.
    tree_body_positions = []
    for opening in RETURNS_VALUE_TREE.finditer(text):
        end_of_parameters = end_of_brackets(text, opening.end() - 1)
        body = BETWEEN_PARAMETERS_AND_BODY.match(text, end_of_parameters)
        if body:
            tree_body_positions.append(body.end() - 1)
    for opening in TRAILING_RETURN_OF_VALUE_TREE.finditer(text):
        tree_body_positions.append(opening.end() - 1)
    for body_position in tree_body_positions:
        end_of_body = end_of_brackets(text, body_position)
        for statement in RETURN_OF_LIST.finditer(text, body_position, end_of_body):
            remember_tree_names(statement.end() - 1, "the return of a ValueTree")

    # 3. Everything in a list of Identifiers.
    for opening in IDENTIFIER_LIST.finditer(text):
        bracket_position = opening.end() - 1
        remember_names(bracket_position, end_of_brackets(text, bracket_position), "a list of Identifiers")

    # 4. XML tag and attribute names. In model/, io/ and render/ every XML
    #    element is the project file; in app/ it can be something else.
    if document_only_layer:
        for opening in XML_OPENING.finditer(text):
            start, end = arguments_of(text, opening.end() - 1, angles_are_brackets=False)[0]
            remember_names(start, end, f"{opening.group(1) or opening.group(2)} (...)")

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

    # Read everything first: a function declared in one file is called in others.
    texts = {}
    for path in source_files(source_root):
        if path.resolve() == ids_file_resolved or is_exempt(path):
            continue
        texts[path] = read_source(path)
    name_functions = name_taking_functions(texts.values())

    for path, text in texts.items():
        layer_path = path.relative_to(source_root).as_posix()
        document_only_layer = layer_path.startswith(DOCUMENT_ONLY_LAYERS)
        for position, rule, string, where in hits_in_file(text, names, document_only_layer, name_functions):
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
