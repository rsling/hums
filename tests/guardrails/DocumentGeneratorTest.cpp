// guardrails::DocumentGenerator: the random valid documents of the property
// tests (docs/GUARDRAILS.md -> 3). That every generated document is valid is
// PropertyTest's question; here it is whether a seed means one document, and
// whether the documents reach all of the schema.

#include <catch2/catch_test_macros.hpp>

#include "DocumentGenerator.h"
#include "io/ProjectFile.h"
#include "model/DeclaredIdentifiers.h"
#include "model/Ids.h"

#include <cstdint>
#include <set>
#include <string>

namespace
{

namespace ids = model::ids;

// Enough documents for everything the generator can build to have occurred;
// each thing below occurs many times among them.
const std::uint64_t seedsLookedAt = 300;

std::string canonical (std::uint64_t seed)
{
    return io::ProjectFile::toCanonicalXml (guardrails::DocumentGenerator::generate (seed)).toStdString();
}

// What occurs in a number of documents.
struct Seen
{
    std::set<juce::String> names; // node types and property names
    std::set<juce::String> eventKinds;
    std::set<juce::String> quantiseValues;
    std::set<juce::String> propertiesHeldAsText;    // properties with a value held as its text
    std::set<juce::String> propertiesHeldAsNumbers; // and with one held as a number or a bool
    int placementsInAutomationLanes = 0;
    int placementsWithAnOffset = 0;
    int gridsWithASkippedColumn = 0;
    int gridsResetBeforeTheirEnd = 0;
    int ticksBeyondAnInt = 0;
};

void look (const juce::ValueTree& node, Seen& seen)
{
    seen.names.insert (node.getType().toString());

    for (int i = 0; i < node.getNumProperties(); ++i)
    {
        const auto property = node.getPropertyName (i);
        const auto& value = node[property];

        seen.names.insert (property.toString());

        if (value.isString())
            seen.propertiesHeldAsText.insert (property.toString());
        else
            seen.propertiesHeldAsNumbers.insert (property.toString());

        if (value.toString().getLargeIntValue() > 2147483647)
            ++seen.ticksBeyondAnInt;
    }

    if (node.hasType (ids::Event))
        seen.eventKinds.insert (node[ids::kind].toString());

    if (node.hasType (ids::Clip) && node.hasProperty (ids::quantise))
        seen.quantiseValues.insert (node[ids::quantise].toString());

    if (node.hasType (ids::Placement))
    {
        if (node.getParent().hasType (ids::AutomationLane))
            ++seen.placementsInAutomationLanes;

        if (node[ids::offset].toString().getLargeIntValue() > 0)
            ++seen.placementsWithAnOffset;
    }

    if (node.hasType (ids::Grid))
    {
        if (node[ids::skipMask].toString().containsChar ('1'))
            ++seen.gridsWithASkippedColumn;

        if (node.hasProperty (ids::resetAt) && node.hasProperty (ids::stepCount) && node[ids::resetAt].toString() != node[ids::stepCount].toString())
            ++seen.gridsResetBeforeTheirEnd;
    }

    for (const auto& child : node)
        look (child, seen);
}

Seen seenInTheFirstDocuments()
{
    Seen seen;

    for (std::uint64_t seed = 1; seed <= seedsLookedAt; ++seed)
        look (guardrails::DocumentGenerator::generate (seed), seen);

    return seen;
}

} // namespace

TEST_CASE ("guardrail 3: the same seed gives the same document, byte for byte", "[guardrails][DocumentGenerator]")
{
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
    {
        INFO ("seed " << seed);

        const auto first = guardrails::DocumentGenerator::generate (seed);
        const auto second = guardrails::DocumentGenerator::generate (seed);

        // Equivalent as trees, down to what each value is held as, and
        // therefore the same text when saved.
        CHECK (first.isEquivalentTo (second));
        CHECK (first.toXmlString() == second.toXmlString());
        CHECK (canonical (seed) == canonical (seed));
    }
}

TEST_CASE ("guardrail 3: every seed gives another document", "[guardrails][DocumentGenerator]")
{
    std::set<std::string> texts;

    for (std::uint64_t seed = 1; seed <= seedsLookedAt; ++seed)
        texts.insert (canonical (seed));

    CHECK (texts.size() == seedsLookedAt);
}

TEST_CASE ("guardrail 3: the generated documents hold every node type and property of Ids.h", "[guardrails][DocumentGenerator]")
{
    // A name added to model/Ids.h fails here until the generator builds it:
    // what the generator does not build, no property test ever sees.
    const auto seen = seenInTheFirstDocuments();

    for (const auto& identifier : model::DeclaredIdentifiers::inDeclarationOrder())
    {
        INFO ("no generated document holds " << identifier.toString());
        CHECK (seen.names.contains (identifier.toString()));
    }
}

TEST_CASE ("guardrail 3: the generated documents hold nothing Ids.h does not declare", "[guardrails][DocumentGenerator]")
{
    // "The generator builds its documents itself, so the unknown properties a
    // file from a newer build may carry do not occur" (docs/GUARDRAILS.md -> 3).
    const auto seen = seenInTheFirstDocuments();

    for (const auto& name : seen.names)
    {
        INFO ("a generated document holds " << name);
        CHECK (model::DeclaredIdentifiers::isDeclared (juce::Identifier (name)));
    }
}

TEST_CASE ("guardrail 3: the generated documents reach the corners of the schema", "[guardrails][DocumentGenerator]")
{
    const auto seen = seenInTheFirstDocuments();

    CHECK (seen.eventKinds == std::set<juce::String> { "cc", "pitchBend", "programChange", "channelPressure", "polyPressure", "sysex" });
    CHECK (seen.quantiseValues == std::set<juce::String> { "", "1/2", "1/4", "1/8", "1/2T", "1/4T", "1/8T" });

    CHECK (seen.placementsInAutomationLanes > 0);
    CHECK (seen.placementsWithAnOffset > 0);
    CHECK (seen.gridsWithASkippedColumn > 0);
    CHECK (seen.gridsResetBeforeTheirEnd > 0);
    CHECK (seen.ticksBeyondAnInt > 0);
}

TEST_CASE ("guardrail 3: a generated value is held as a number in one place and as its text in another", "[guardrails][DocumentGenerator]")
{
    // A command of this session sets a number, a loaded file holds its text,
    // and the document must read the same either way. A few properties stand
    // for the rest. The two doubles are not among them: the generator holds
    // them as text only (DocumentGenerator.cpp, setDouble).
    const auto seen = seenInTheFirstDocuments();

    for (const auto& property : { ids::tick, ids::pitch, ids::on, ids::enabled, ids::loopLength })
    {
        INFO (property.toString());
        CHECK (seen.propertiesHeldAsText.contains (property.toString()));
        CHECK (seen.propertiesHeldAsNumbers.contains (property.toString()));
    }
}
