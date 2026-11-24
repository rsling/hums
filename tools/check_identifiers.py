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

and the other calls in IDENTIFIER_CALLS. The variable may be a member defined
outside its class: const juce::Identifier NoteModel::pitch ("pitch"). The name is the first argument of
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
    PropertyRule rule { "Note", "pitch", 0 }      a member of a struct of ours
    rows.push_back ({ slot, "scene", 0 })         that is a juce::Identifier
    RowKey::RowKey() : rowProperty ("scene")
    key.rowProperty = "scene"

The functions of ours are not a list kept by hand: the check reads every
declaration under the source root and remembers which parameters of which
function are a juce::Identifier, as it reads the names from Ids.h. A call is
matched by the function's name alone, whichever class it belongs to, and with
or without template arguments: readAs<int> (node, "pitch").

The structs of ours are read the same way: the check reads every struct and
class under the source root and remembers which of its members are a
juce::Identifier (or a list of them), by name and, for a struct that is
filled from a list in braces, by position. A list in braces is checked at
exactly those positions, so a value beside the name is still not a name. A
member is matched by its name alone, whichever struct it belongs to.

Reported with file:line and a rule name in square brackets:

  [identifier]  a schema name spelled as a string outside Ids.h
  [literal-name] in model/, io/ and render/ only: any string literal, schema
                name or not, as the name in one of those places: the first
                argument of a call, the names of a construction, the
                initialiser of an Identifier, a subscript, and the
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

# The variable of a declaration: "pitch", or, where a static member is defined
# outside its class, "NoteModel::pitch".
DECLARED_NAME = r"(?:\w+\s*::\s*)*\w+"

STRING_LITERAL = re.compile(r'"((?:[^"\\\n]|\\.)*)"')

CALL_OPENING = re.compile(r"\b(" + "|".join(IDENTIFIER_CALLS) + r")\s*\(")
CONSTRUCTION_OPENING = re.compile(
    r"\b(" + "|".join(IDENTIFIER_TYPES) + r")\b\s*(?:" + DECLARED_NAME + r"\s*)?[({]"
)
ASSIGNMENT = re.compile(
    r"\b(" + "|".join(IDENTIFIER_TYPES) + r")\b\s*" + DECLARED_NAME + r"\s*=\s*" + STRING_LITERAL.pattern
)
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
    r"\bIdentifier\b\s*(?:,[^<>;(){}]*)?>\s*(?:const\s+)?(?:" + DECLARED_NAME + r"\s*)?(?:=\s*)?[({]"
    r"|\bIdentifier\b\s+" + DECLARED_NAME + r"\s*\[[^\]]*\]\s*(?:=\s*)?\{"
)

#    What may stand between the name of a function of ours and its bracket in
#    a call: template arguments, as in "readAs<int> (node, ids::pitch)".
TEMPLATE_ARGUMENTS = r"<[^;(){}]*>"

# 5. A member of a struct or class of ours that is a juce::Identifier, or a
#    list of them. "struct PropertyRule {" and "class RowKey final : public Base {"
#    open a body; "enum class ValueKind {" does not, and is told apart by the
#    word before it.
CLASS_OPENING = re.compile(r"\b(?:struct|class)\s+(\w+)\s*(?:final\s*)?(:(?!:)[^;{]*)?\{")
ENUM_BEFORE = re.compile(r"\benum\s+$")
CLASS_KEYWORD_BEFORE = re.compile(r"\b(?:struct|class)\s+$")
ACCESS_SPECIFIER = re.compile(r"\s*(?:public|private|protected)\s*:(?!:)")
OPERATOR = re.compile(r"\boperator\b")

#    What stands directly in a class body without being a data member that a
#    list in braces fills.
NOT_A_DATA_MEMBER = re.compile(r"(?:using|typedef|friend|static|struct|class|enum|union|template)\b")

#    "juce::Identifier rowProperty", "int row", "std::vector<int> rows[4]":
#    the type and the name of a data member, its initialiser cut off.
DATA_MEMBER = re.compile(r"(.*[\s&*>])(\w+)\s*(?:\[[^\]]*\]\s*)?", re.DOTALL)
IDENTIFIER_MEMBER_TYPE = re.compile(
    r"(?:(?:const|mutable)\s+)*(?:juce\s*::\s*)?Identifier"
    r"|.*\bIdentifier\b\s*(?:,[^<>]*)?>",
    re.DOTALL,
)

#    The calls that give a container one more element: "rows.push_back ({ ... })"
#    takes the element as a list in braces, "rows.emplace_back (...)" takes
#    what would stand in the braces.
ELEMENT_CALLS = ("push_back", "emplace_back")

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


def declarations_in_class(text, body_position):
    """(text, whether it is a function) of everything that stands directly in
    the class body whose brace is at body_position, in order.

    A declaration ends at its semicolon; a function that is defined in the
    class ends at the brace that closes its body. What has round brackets
    before any "=" is a function, and so is every operator.
    """
    end_of_body = end_of_brackets(text, body_position) - 1
    declarations = []
    start = position = body_position + 1
    has_parameters = False
    has_initialiser = False

    def is_function():
        return has_parameters or OPERATOR.search(text, start, position) is not None

    while position < end_of_body:
        character = text[position]
        if character == '"':
            literal = STRING_LITERAL.match(text, position)
            position = literal.end() if literal else position + 1
        elif character == "(":
            has_parameters = has_parameters or not has_initialiser
            position = end_of_brackets(text, position)
        elif character == "{":
            function = is_function()
            position = end_of_brackets(text, position)
            # After a constructor's "member { ... }" comes the next member or
            # the body; only the body ends the function.
            following = text[position:end_of_body].lstrip()[:1]
            if function and following not in ("{", ","):
                declarations.append((text[start:position], True))
                start = position
                has_parameters = has_initialiser = False
        elif character == ";":
            declarations.append((text[start:position], is_function()))
            position += 1
            start = position
            has_parameters = has_initialiser = False
        else:
            has_initialiser = has_initialiser or character == "="
            position += 1
    return declarations


def identifier_members_of_class(class_name, has_base, text, body_position):
    """(the names of the members that are an Identifier, their positions among
    the data members), read from the class body whose brace is at body_position.

    The positions are those a list in braces fills, { first, second, ... }.
    They are None for a class that is not filled so: one with a constructor
    of its own, whose arguments name_taking_functions() knows, and one with a
    base class, whose members come first and are not read here.
    """
    member_names = set()
    positions = set()
    filled_from_a_list = not has_base
    constructor = re.compile(r"(?:(?:explicit|constexpr|inline)\s+)*" + re.escape(class_name) + r"\s*\(")
    index = 0

    for declaration, is_function in declarations_in_class(text, body_position):
        while ACCESS_SPECIFIER.match(declaration):
            declaration = declaration[ACCESS_SPECIFIER.match(declaration).end():]
        declaration = declaration.strip()
        if is_function:
            if constructor.match(declaration):
                filled_from_a_list = False
            continue
        if not declaration or NOT_A_DATA_MEMBER.match(declaration):
            continue
        member = DATA_MEMBER.fullmatch(re.split(r"[={]", declaration, maxsplit=1)[0])
        if not member:
            continue
        if IDENTIFIER_MEMBER_TYPE.fullmatch(member.group(1).strip()):
            member_names.add(member.group(2))
            positions.add(index)
        index += 1

    return member_names, (positions if filled_from_a_list else None)


class StructsWithNames:
    """What the sources say about our own structs and classes with a member
    that is a juce::Identifier or a list of them.

    Only names are kept, as for the functions: two structs of one name are
    told apart by nothing here, nor are two members of one name in different
    structs.
    """

    def __init__(self, texts):
        # The names of the members that are an Identifier, of every struct and class.
        self.member_names = set()
        # {struct name: the positions of its Identifier members}, for the
        # structs that a list in braces fills.
        self.positions = {}
        # {variable name: the structs it is a list of}: "std::vector<NodeInARow> found".
        self.lists = {}

        for text in texts:
            for opening in CLASS_OPENING.finditer(text):
                if ENUM_BEFORE.search(text, max(0, opening.start() - 20), opening.start()):
                    continue
                member_names, positions = identifier_members_of_class(
                    opening.group(1), opening.group(2) is not None, text, opening.end() - 1
                )
                self.member_names |= member_names
                if positions:
                    self.positions.setdefault(opening.group(1), set()).update(positions)

        if self.positions:
            list_variable = re.compile(self.list_of_structs() + r"(\w+)\s*[;={(,)]")
            for text in texts:
                for declaration in list_variable.finditer(text):
                    self.lists.setdefault(declaration.group(2), set()).add(declaration.group(1))

    def struct_names(self):
        return r"\b(" + "|".join(sorted(self.positions)) + r")\b"

    def list_of_structs(self):
        """The type of a list of one of the structs, up to where a name follows:
        "std::vector<PropertyRule> ", "const std::array<PropertyRule, 2>& "."""
        return self.struct_names() + r"\s*(?:,[^<>;(){}]*)?>\s*(?:const\s+)?&?\s*"


def hits_in_file(text, names, document_only_layer, name_functions, structs):
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
    #    The optional name is the variable in "PropertyName name (...)", for a
    #    constructor that takes one; it is qualified where a static member is
    #    defined outside its class.
    if name_functions:
        own_call_opening = re.compile(
            r"\b(" + "|".join(sorted(name_functions)) + r")\b\s*"
            r"(?:" + TEMPLATE_ARGUMENTS + r"\s*)?(?:" + DECLARED_NAME + r"\s*)?[({]"
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

    # 5. The members of our own structs that are an Identifier.
    def remember_struct_names(bracket_position, struct, where):
        """The names in the brackets that fill one struct: { first, second, ... }."""
        arguments = arguments_of(text, bracket_position, angles_are_brackets=False)
        for index in sorted(structs.positions[struct]):
            if index < len(arguments):
                remember_names(*arguments[index], where)

    def remember_struct_list_names(brace_position, struct, where):
        """The same for every element of { { ... }, { ... } } that is a list in braces."""
        for element_position in bracketed_elements(text, brace_position, end_of_brackets(text, brace_position)):
            if text[element_position] == "{":
                remember_struct_names(element_position, struct, where)

    def bodies_of_functions_returning(return_type):
        """The position of the body's brace of every function defined with that return type."""
        positions = []
        for opening in re.compile(return_type + r"(?:\w+\s*::\s*)*\w+\s*\(").finditer(text):
            body = BETWEEN_PARAMETERS_AND_BODY.match(text, end_of_brackets(text, opening.end() - 1))
            if body:
                positions.append((opening.group(1), body.end() - 1))
        return positions

    if structs.member_names:
        member = r"\b(" + "|".join(sorted(structs.member_names)) + r")\b"

        # In a constructor: "RowKey::RowKey() : rowProperty ("scene"), other { "index" }".
        for opening in re.compile(r"(?<!:)[:,]\s*" + member + r"\s*[({]").finditer(text):
            bracket_position = opening.end() - 1
            remember_names(
                bracket_position, end_of_brackets(text, bracket_position), f"the member {opening.group(1)}"
            )

        # Assigned: "key.rowProperty = "scene";", "rowProperty = "scene";" as a
        # statement of its own, and "{ .rowProperty = "scene" }" in a list in braces.
        assigned = re.compile(r"(?:\.|->|[;{}]\s*)" + member + r"\s*=(?!=)\s*" + STRING_LITERAL.pattern)
        for assignment in assigned.finditer(text):
            remember(assignment.start(2) - 1, assignment.group(2), f"the member {assignment.group(1)}")

    if structs.positions:
        # One struct: "PropertyRule rule { ... }", "PropertyRule { ... }",
        # "PropertyRule rule = { ... }". Not the struct's own definition, and
        # not a function that returns one, whose brackets hold parameters.
        one_struct = re.compile(
            structs.struct_names() + r"\s*(?:" + DECLARED_NAME + r"\s*)?(?:=\s*)?[({]"
        )
        for opening in one_struct.finditer(text):
            bracket_position = opening.end() - 1
            if CLASS_KEYWORD_BEFORE.search(text, max(0, opening.start() - 20), opening.start()):
                continue
            if text[bracket_position] == "(" and BETWEEN_PARAMETERS_AND_BODY.match(
                text, end_of_brackets(text, bracket_position)
            ):
                continue
            remember_struct_names(bracket_position, opening.group(1), f"a {opening.group(1)}")

        # A list of them: "std::vector<PropertyRule> rules { { ... }, { ... } }",
        # "PropertyRule rules[] = { ... }".
        list_of_structs = re.compile(
            structs.list_of_structs() + r"(?:" + DECLARED_NAME + r"\s*)?(?:=\s*)?\{"
            r"|" + structs.struct_names() + r"\s+" + DECLARED_NAME + r"\s*\[[^\]]*\]\s*(?:=\s*)?\{"
        )
        for opening in list_of_structs.finditer(text):
            struct = opening.group(1) or opening.group(2)
            remember_struct_list_names(opening.end() - 1, struct, f"a list of {struct}")

        # What a function returns as one, or as a list of them.
        for struct, body_position in bodies_of_functions_returning(structs.struct_names() + r"\s*&?\s*"):
            for statement in RETURN_OF_LIST.finditer(text, body_position, end_of_brackets(text, body_position)):
                remember_struct_names(statement.end() - 1, struct, f"the return of a {struct}")
        for struct, body_position in bodies_of_functions_returning(structs.list_of_structs()):
            for statement in RETURN_OF_LIST.finditer(text, body_position, end_of_brackets(text, body_position)):
                remember_struct_list_names(statement.end() - 1, struct, f"the return of a list of {struct}")

    if structs.lists:
        # One more element for a list of them, named by the variable that is
        # the list: "found.push_back ({ ... })", "found.emplace_back (...)".
        element_call = re.compile(
            r"\b(" + "|".join(sorted(structs.lists)) + r")\s*(?:\.|->)\s*"
            r"(" + "|".join(ELEMENT_CALLS) + r")\s*\("
        )
        for opening in element_call.finditer(text):
            bracket_position = opening.end() - 1
            for struct in sorted(structs.lists[opening.group(1)]):
                where = f"{opening.group(2)} (...) of a {struct}"
                if opening.group(2) == "emplace_back":
                    remember_struct_names(bracket_position, struct, where)
                    continue
                start, end = arguments_of(text, bracket_position, angles_are_brackets=False)[0]
                element =start + len(text[start:end]) - len(text[start:end].lstrip())
                if element < end and text[element] == "{":
                    remember_struct_names(element, struct, where)

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
    structs = StructsWithNames(list(texts.values()))

    for path, text in texts.items():
        layer_path = path.relative_to(source_root).as_posix()
        document_only_layer = layer_path.startswith(DOCUMENT_ONLY_LAYERS)
        for position, rule, string, where in hits_in_file(
            text, names, document_only_layer, name_functions, structs
        ):
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
