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


def _comments_and_literals(text):
    """Yields (kind, start, end) for every comment and every string or
    character literal, in the order they stand; kind is "comment" or
    "literal". A // inside a string literal is not a comment, and a quote
    inside a comment opens no literal.
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


def literal_spans(text):
    """Where the string and character literals are: a list of (start, end),
    quotes included, in the order they stand.

    A bracket or a call name inside a literal is text, not code.
    """
    return [(start, end) for kind, start, end in _comments_and_literals(text) if kind == "literal"]
