// io::XmlWellFormedness: what the load path asks of a project file's text
// before JUCE's parser reads it (docs/SCHEMA.md -> Loading, Parse).

#include <catch2/catch_test_macros.hpp>

#include "io/XmlWellFormedness.h"

#include <string>
#include <vector>

namespace
{

using Kind = io::XmlWellFormedness::Result::Kind;

Kind kindOf (const juce::String& text)
{
    return io::XmlWellFormedness::check (text).kind;
}

std::string reasonOf (const juce::String& text)
{
    return io::XmlWellFormedness::check (text).reason.toStdString();
}

} // namespace

TEST_CASE ("Well-formed XML passes, and has no reason", "[io][XmlWellFormedness]")
{
    const std::vector<juce::String> texts {
        "<a/>",
        "<a></a>",
        "<a><b/><c><d/></c></a>",
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\n<a>\n  <b/>\n</a>\n",
        "<!-- before --><a/><!-- after -->\n",
        "<a><!-- </b> --><b/></a>",
        "<a b=\"1\" c='2'/>",
        "<a b = \"1\" ></a >",
        // A > and a quote of the other kind are plain text inside a value,
        // and so is what looks like a tag.
        "<a b=\"x > y\" c='say \"</a>\"'/>",
        "<a>text <b/> more text</a>",
        "<a><![CDATA[ </a> ]]></a>",
        "<a><?instruction </a> ?></a>",
    };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        CHECK (kindOf (text) == Kind::wellFormed);
        CHECK (reasonOf (text).empty());
    }
}

TEST_CASE ("A closing tag closes the node that is open, under its name", "[io][XmlWellFormedness]")
{
    CHECK (kindOf ("<a></b>") == Kind::notWellFormed);
    CHECK (reasonOf ("<a></b>") == "the a node is closed as b");

    CHECK (kindOf ("<a><b></a></b>") == Kind::notWellFormed);
    CHECK (reasonOf ("<a><b></a></b>") == "the b node is closed as a");

    // Names are compared as written.
    CHECK (kindOf ("<a></A>") == Kind::notWellFormed);
}

TEST_CASE ("A closing tag is complete", "[io][XmlWellFormedness]")
{
    const std::vector<juce::String> texts { "<a></a", "<a></", "<a></>", "<a></a/>", "<a></a b>", "<abc></ab" };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        CHECK (kindOf (text) == Kind::notWellFormed);
    }

    CHECK (reasonOf ("<a></a") == "the closing tag of the a node is not complete");
}

TEST_CASE ("Nothing but comments and processing instructions stands outside the root", "[io][XmlWellFormedness]")
{
    const std::vector<juce::String> texts {
        "<a/>text",
        "<a/><b/>",
        "<a></a><a></a>",
        "<a/></a>",
        "text<a/>",
        "<a/><![CDATA[ text ]]>",
        // A DOCTYPE stands before the root or nowhere.
        "<a/><!DOCTYPE a>",
    };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        CHECK (kindOf (text) == Kind::notWellFormed);
    }

    CHECK (reasonOf ("<a/><b/>") == "something follows the end of its root node");
}

TEST_CASE ("A text without a node is not well-formed", "[io][XmlWellFormedness]")
{
    const std::vector<juce::String> texts { "", "  \n", "<?xml version=\"1.0\"?>", "<!-- only a comment -->", "no XML at all" };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        CHECK (kindOf (text) == Kind::notWellFormed);
    }
}

TEST_CASE ("What is open where the text ends is not well-formed", "[io][XmlWellFormedness]")
{
    const std::vector<juce::String> texts {
        "<a>",
        "<a><b/>",
        "<a",
        "<a b=\"1\"",
        "<a b=\"1/>",
        "<a><!-- comment</a>",
        "<a><![CDATA[ text</a>",
        "<a><?instruction</a>",
        "<!-- comment <a/>",
    };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        CHECK (kindOf (text) == Kind::notWellFormed);
    }

    CHECK (reasonOf ("<a><b>") == "it ends inside the b node");
}

TEST_CASE ("An opening tag has a name, and its attributes have values in quotes", "[io][XmlWellFormedness]")
{
    const std::vector<juce::String> texts { "<>", "< a/>", "<a b/>", "<a b=1/>", "<a b=/>", "<a =\"1\"/>", "<a <!-- comment --> b=\"1\"/>", "<a / >" };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        CHECK (kindOf (text) == Kind::notWellFormed);
    }

    CHECK (reasonOf ("<a b=1/>") == "the attribute b of a a node has no value in quotes");
}

TEST_CASE ("A DOCTYPE before the root is reported as one, whatever it holds", "[io][XmlWellFormedness]")
{
    const std::vector<juce::String> texts {
        "<!DOCTYPE a><a/>",
        "<?xml version=\"1.0\"?>\n<!-- comment -->\n<!DOCTYPE a [ <!ENTITY b \"c\"> ]>\n<a/>",
        "<!DOCTYPE a",
    };

    for (const auto& text : texts)
    {
        INFO (text.toStdString());
        CHECK (kindOf (text) == Kind::hasDoctype);
    }

    // In a comment it is none.
    CHECK (kindOf ("<!-- <!DOCTYPE a> --><a/>") == Kind::wellFormed);
}
