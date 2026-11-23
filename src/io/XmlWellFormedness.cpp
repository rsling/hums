#include "io/XmlWellFormedness.h"

#include <cstring>
#include <optional>
#include <vector>

namespace io
{

namespace
{

    using Result = XmlWellFormedness::Result;
    using TextPosition = juce::String::CharPointerType;

    // What is wrong, or nothing.
    using Problem = std::optional<juce::String>;

    bool startsWith (TextPosition text, const char* start)
    {
        return juce::CharacterFunctions::compareUpTo (text, juce::CharPointer_ASCII (start), static_cast<int> (std::strlen (start))) == 0;
    }

    // Moves behind the next occurrence of `end`. False, and nothing moved,
    // when there is none.
    bool skipPast (TextPosition& text, const char* end)
    {
        const auto index = text.indexOf (juce::CharPointer_ASCII (end));

        if (index < 0)
            return false;

        text += index + static_cast<int> (std::strlen (end));

        return true;
    }

    // What may stand between nodes, and before and after the root: a comment
    // or a processing instruction (the XML header is one). True when one
    // begins at `text`; it is then skipped, or reported as not closed.
    bool skipCommentOrInstruction (TextPosition& text, Problem& problem)
    {
        if (startsWith (text, "<!--"))
        {
            if (! skipPast (text, "-->"))
                problem = "a comment is not closed";

            return true;
        }

        if (startsWith (text, "<?"))
        {
            if (! skipPast (text, "?>"))
                problem = "a processing instruction is not closed";

            return true;
        }

        return false;
    }

    // Skips whitespace, comments and processing instructions.
    Problem skipWhatMayStandOutsideTheRoot (TextPosition& text)
    {
        for (;;)
        {
            text.incrementToEndOfWhitespace();

            Problem problem;

            if (! skipCommentOrInstruction (text, problem))
                return std::nullopt;

            if (problem.has_value())
                return problem;
        }
    }

    // The name of a node or an attribute ends where the syntax of a tag goes
    // on. Whether its characters are ones a name may have is JUCE's parser's
    // to say.
    bool endsAName (juce::juce_wchar character)
    {
        return character == 0 || juce::CharacterFunctions::isWhitespace (character) || character == '<' || character == '>' || character == '/'
            || character == '=' || character == '"' || character == '\'';
    }

    juce::String readName (TextPosition& text)
    {
        const auto start = text;

        while (! endsAName (*text))
            ++text;

        return juce::String (start, text);
    }

    // Reads an opening tag from its "<" to its ">" or "/>". A node that
    // stays open is added to `openNodes`.
    Problem readOpeningTag (TextPosition& text, std::vector<juce::String>& openNodes)
    {
        ++text;
        const auto nodeName = readName (text);

        if (nodeName.isEmpty())
            return juce::String ("a < is followed by no name");

        const auto notComplete = "the tag of a " + nodeName + " node is not complete";

        for (;;)
        {
            text.incrementToEndOfWhitespace();

            if (*text == '>')
            {
                ++text;
                openNodes.push_back (nodeName);
                return std::nullopt;
            }

            if (startsWith (text, "/>"))
            {
                text += 2;
                return std::nullopt;
            }

            const auto attributeName = readName (text);

            if (attributeName.isEmpty())
                return notComplete;

            const auto noValue = "the attribute " + attributeName + " of a " + nodeName + " node has no value in quotes";

            text.incrementToEndOfWhitespace();

            if (*text != '=')
                return noValue;

            ++text;
            text.incrementToEndOfWhitespace();

            const auto quote = *text;

            if (quote != '"' && quote != '\'')
                return noValue;

            ++text;

            while (*text != quote)
            {
                if (text.isEmpty())
                    return notComplete;

                ++text;
            }

            ++text;
        }
    }

    // Reads a closing tag from its "</" to its ">", and takes the node it
    // closes from `openNodes`, which is not empty.
    Problem readClosingTag (TextPosition& text, std::vector<juce::String>& openNodes)
    {
        const auto& openNode = openNodes.back();

        text += 2;
        const auto closedName = readName (text);

        if (closedName.isNotEmpty() && closedName != openNode)
            return "the " + openNode + " node is closed as " + closedName;

        text.incrementToEndOfWhitespace();

        if (closedName.isEmpty() || *text != '>')
            return "the closing tag of the " + openNode + " node is not complete";

        ++text;
        openNodes.pop_back();

        return std::nullopt;
    }

    // Reads the root node, from its "<" to the end of its closing tag, with
    // everything in it.
    Problem readRootNode (TextPosition& text)
    {
        std::vector<juce::String> openNodes;

        if (const auto problem = readOpeningTag (text, openNodes))
            return problem;

        while (! openNodes.empty())
        {
            if (text.isEmpty())
                return "it ends inside the " + openNodes.back() + " node";

            Problem problem;

            if (skipCommentOrInstruction (text, problem))
            {
                // Skipped, or the problem is set.
            }
            else if (startsWith (text, "<![CDATA["))
            {
                if (! skipPast (text, "]]>"))
                    problem = "a CDATA section is not closed";
            }
            else if (startsWith (text, "</"))
            {
                problem = readClosingTag (text, openNodes);
            }
            else if (*text == '<')
            {
                problem = readOpeningTag (text, openNodes);
            }
            else
            {
                // Text between nodes. Whether it may be there is not asked
                // here.
                ++text;
            }

            if (problem.has_value())
                return problem;
        }

        return std::nullopt;
    }

    Result notWellFormed (const juce::String& reason)
    {
        return { Result::Kind::notWellFormed, reason };
    }

} // namespace

XmlWellFormedness::Result XmlWellFormedness::check (const juce::String& xmlText)
{
    auto text = xmlText.getCharPointer();

    if (const auto problem = skipWhatMayStandOutsideTheRoot (text))
        return notWellFormed (*problem);

    if (startsWith (text, "<!DOCTYPE"))
        return { Result::Kind::hasDoctype, "it has a DOCTYPE" };

    if (text.isEmpty())
        return notWellFormed ("it holds no node");

    if (*text != '<')
        return notWellFormed ("there is text before its first node");

    if (const auto problem = readRootNode (text))
        return notWellFormed (*problem);

    if (const auto problem = skipWhatMayStandOutsideTheRoot (text))
        return notWellFormed (*problem);

    if (! text.isEmpty())
        return notWellFormed ("something follows the end of its root node");

    return {};
}

} // namespace io
