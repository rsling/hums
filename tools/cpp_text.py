"""Shared by the guardrail scripts: reading C++ source as text.

Not a parser. The scripts look at include lines and at string literals inside
a few named calls; for that it is enough to know what is a comment.
"""


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
