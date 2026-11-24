// A clip holds the content of its own kind and no other: an event clip an
// Events, a step clip a Grid (docs/SCHEMA.md -> Tree). Finding 17 of the
// milestone 1 code review (23 Nov 2026): a project file in which a step clip
// had an Events child, or an event clip a Grid, loaded as valid, although no
// command makes such a clip. The Validator refuses both now (invariant 12).
//
// Each case is all_node_types.xml, a valid fixture, with one child added.

#include <catch2/catch_test_macros.hpp>

#include "io/ProjectFile.h"
#include "model/Ids.h"

#include <string>

namespace
{

namespace ids = model::ids;
using Outcome = io::ProjectFile::LoadResult::Outcome;

juce::String validFixtureText()
{
    const auto file = juce::File (HUMS_IO_FIXTURES_DIRECTORY).getChildFile ("all_node_types.xml");
    REQUIRE (file.existsAsFile());

    return file.loadFileAsString();
}

// The fixture with `added` as one more child of its first clip of that kind:
// behind the Grid of the step clip, or behind the Events of the first event
// clip.
juce::String fixtureWithChildBehind (const juce::String& closingTag, const juce::String& added)
{
    const auto text = validFixtureText();
    const auto endOfClip = closingTag + "\n    </Clip>";
    const auto changed = text.replaceFirstOccurrenceOf (endOfClip, closingTag + "\n      " + added + "\n    </Clip>");
    REQUIRE (changed != text);

    return changed;
}

struct Loaded
{
    juce::ValueTree document;
    juce::UndoManager undoManager;
    io::ProjectFile::LoadResult result;

    explicit Loaded (const juce::String& text)
        : result (io::ProjectFile::loadFromText (text, document, undoManager))
    {
    }

    // What the load made of it, for the report of a failed check.
    std::string description() const
    {
        if (! result.wasLoaded())
            return "refused: " + result.message.toStdString();

        std::string text = "loaded; the clips hold:";

        for (const auto& clip : document.getChildWithName (ids::ClipPool))
        {
            text += "\n  " + clip[ids::kind].toString().toStdString() + " clip:";

            for (const auto& child : clip)
                text += " " + child.getType().toString().toStdString() + " (" + std::to_string (child.getNumChildren()) + " children)";
        }

        return text;
    }
};

} // namespace

TEST_CASE ("Finding 17, the control: the fixture as it stands loads", "[io][ProjectFile][load]")
{
    const Loaded loaded (validFixtureText());

    INFO (loaded.description());
    CHECK (loaded.result.outcome == Outcome::loaded);
}

TEST_CASE ("Finding 17: a step clip with an Events child is refused", "[io][ProjectFile][load]")
{
    SECTION ("an empty Events")
    {
        const Loaded loaded (fixtureWithChildBehind ("</Grid>", "<Events/>"));

        INFO (loaded.description());
        CHECK (loaded.result.outcome == Outcome::invalidDocument);
    }

    SECTION ("an Events with a note")
    {
        const Loaded loaded (fixtureWithChildBehind ("</Grid>", R"(<Events><Note tick="0" length="240" pitch="60"/></Events>)"));

        INFO (loaded.description());
        CHECK (loaded.result.outcome == Outcome::invalidDocument);
    }
}

TEST_CASE ("Finding 17: an event clip with a Grid child is refused", "[io][ProjectFile][load]")
{
    SECTION ("an empty Grid")
    {
        const Loaded loaded (fixtureWithChildBehind ("</Events>", "<Grid/>"));

        INFO (loaded.description());
        CHECK (loaded.result.outcome == Outcome::invalidDocument);
    }

    SECTION ("a Grid with a lane and its cells")
    {
        const Loaded loaded (fixtureWithChildBehind (
            "</Events>",
            R"(<Grid stepCount="2"><Lane id="0f1e2d3c4b5a49788796a5b4c3d2e1f0"><Cell on="1"/><Cell/></Lane></Grid>)"));

        INFO (loaded.description());
        CHECK (loaded.result.outcome == Outcome::invalidDocument);
    }
}
