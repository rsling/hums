"""Shared by the guardrail scripts: reading C++ source as text.

Not a parser. The scripts look at include lines and at string literals inside
a few named calls; for that it is enough to know what is a comment and what
is a string or character literal.
"""

NUMBER_CHARACTERS = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.'"


def _is_digit_separator(text, index):
    """Whether the ' at index stands inside a number, as in 1'000 or 0xFF'FF.

    It does when the word it stands in begins with a digit and goes on after
    it. The u8 of u8'a' is a word that begins with a letter, so that ' opens
    a character literal.
    """
    following = text[index + 1] if index + 1 < len(text) else ""
    if not (following.isascii() and following.isalnum()):
        return False
    start = index
    while start > 0 and text[start - 1] in NUMBER_CHARACTERS:
        start -= 1
    word = text[start:index].lstrip(".")
    return word[:1].isdigit()


# What stands before the opening quote of a raw string literal.
RAW_STRING_PREFIXES = {"R", "LR", "uR", "UR", "u8R"}

# A raw string's delimiter is at most this long (the C++ standard).
LONGEST_RAW_STRING_DELIMITER = 16


def _raw_string_end(text, index):
    """Where the raw string literal ends whose opening quote is at index, or
    None when that quote opens an ordinary string.

    A raw string is R"delimiter(text)delimiter", the delimiter often empty.
    Its text ends at the closing bracket with the delimiter and a quote
    behind it and nowhere else: a quote, a backslash, a line break and a /*
    in it are text. Read as an ordinary string, R"(" /*)" would be the
    string "(" and the start of a comment.
    """
    start = index
    while start > 0 and (text[start - 1].isalnum() or text[start - 1] == "_"):
        start -= 1
    if text[start:index] not in RAW_STRING_PREFIXES:
        return None

    bracket = text.find("(", index + 1)
    if bracket == -1 or bracket - index - 1 > LONGEST_RAW_STRING_DELIMITER:
        return None
    delimiter = text[index + 1 : bracket]
    if any(character in ' \t\n\v\f)\\"' for character in delimiter):
        return None

    closing = text.find(")" + delimiter + '"', bracket + 1)
    if closing == -1:
        return len(text)
    return closing + len(delimiter) + 2


def join_spliced_lines(text):
    """Joins every line that ends in a backslash with the line after it, as
    the compiler does before it reads anything else, and keeps every line
    number: the lines that went into one are followed by as many empty ones.

    After this, a directive that was written over several lines stands on the
    line it began on. Blanks between the backslash and the line break are
    allowed, because the compiler allows them.
    """
    result = []
    joined = None
    empty_lines_owed = 0
    for line in text.split("\n"):
        without_blanks = line.rstrip(" \t")
        is_spliced = without_blanks.endswith("\\")
        part = without_blanks[:-1] if is_spliced else line
        joined = part if joined is None else joined + part
        if is_spliced:
            empty_lines_owed += 1
            continue
        result.append(joined)
        result.extend([""] * empty_lines_owed)
        joined = None
        empty_lines_owed = 0

    if joined is not None:
        # The last line of the text ended in a backslash.
        result.append(joined)
        result.extend([""] * (empty_lines_owed - 1))
    return "\n".join(result)


def _comments_and_literals(text):
    """Yields (kind, start, end) for every comment and every string or
    character literal, in the order they stand; kind is "comment" or
    "literal". A // inside a string literal is not a comment, and a quote
    inside a comment opens no literal. A raw string literal is one literal
    from its opening quote to its closing one, over however many lines.
    """
    index = 0
    length = len(text)
    while index < length:
        character = text[index]
        following = text[index + 1] if index + 1 < length else ""
        if character == "/" and following == "/":
            end = text.find("\n", index)
            end = length if end == -1 else end
            yield "comment", index, end
            index = end
        elif character == "/" and following == "*":
            end = text.find("*/", index + 2)
            end = length if end == -1 else end + 2
            yield "comment", index, end
            index = end
        elif character == '"' and _raw_string_end(text, index) is not None:
            end = _raw_string_end(text, index)
            yield "literal", index, end
            index = end
        elif character == '"' or (character == "'" and not _is_digit_separator(text, index)):
            end = index + 1
            while end < length and text[end] != character and text[end] != "\n":
                end += 2 if text[end] == "\\" else 1
            end = min(end + 1, length)
            yield "literal", index, end
            index = end
        else:
            index += 1


def blank_out_comments(text):
    """Replaces comment text with spaces; keeps line breaks and string literals.

    An include line inside a comment is not an include, and a // inside a
    string literal is not a comment.
    """
    result = []
    position = 0
    for kind, start, end in _comments_and_literals(text):
        if kind == "comment":
            result.append(text[position:start])
            result.append("".join("\n" if c == "\n" else " " for c in text[start:end]))
            position = end
    result.append(text[position:])
    return "".join(result)


def blank_out_character_literals(text):
    """Replaces what stands between the quotes of every character literal
    with spaces; keeps the quotes, string literals and every position.

    A bracket, a comma or a quote in a character literal is a character, not
    code: after this, '}' closes nothing and ',' separates nothing for a
    reader that counts brackets. A literal that its line does not close is
    left as it is.
    """
    result = []
    position = 0
    for kind, start, end in _comments_and_literals(text):
        closed = end - start >= 2 and text[end - 1] == "'"
        if kind == "literal" and text[start] == "'" and closed:
            result.append(text[position:start + 1])
            result.append(" " * (end - start - 2))
            position = end - 1
    result.append(text[position:])
    return "".join(result)


def literal_spans(text):
    """Where the string and character literals are: a list of (start, end),
    quotes included, in the order they stand.

    A bracket or a call name inside a literal is text, not code.
    """
    return [(start, end) for kind, start, end in _comments_and_literals(text) if kind == "literal"]
