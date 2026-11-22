// model::commands::SiblingOrder: a node moved among its siblings of the same
// type. The commands that use it (moveTrack, moveKnob, moveAutomationLane)
// have tests of their own; here is what all of them rely on.

#include <catch2/catch_test_macros.hpp>

#include "model/Ids.h"
#include "model/commands/SiblingOrder.h"

#include <string>

namespace
{

namespace ids = model::ids;
using model::commands::SiblingOrder;

// Knobs 1, 2 and 3 with a node of another type between the first two.
juce::ValueTree knobsWithAStranger()
{
    juce::ValueTree knobs (ids::Knobs);

    for (const auto cc : { 1, 2, 3 })
    {
        juce::ValueTree knob (ids::Knob);
        knob.setProperty (ids::cc, cc, nullptr);
        knobs.appendChild (knob, nullptr);
    }

    knobs.addChild (juce::ValueTree (ids::Marker), 1, nullptr);

    return knobs;
}

// The children in order: a knob as its number, anything else as "?".
std::string orderOf (const juce::ValueTree& knobs)
{
    std::string text;

    for (const auto& child : knobs)
        text += child.hasType (ids::Knob) ? child[ids::cc].toString().toStdString() : std::string ("?");

    return text;
}

} // namespace

TEST_CASE ("SiblingOrder moves a node to a position counted among its own kind, and undo takes it back", "[model][commands][SiblingOrder]")
{
    auto knobs = knobsWithAStranger();
    juce::UndoManager undoManager;
    REQUIRE (orderOf (knobs) == "1?23");

    undoManager.beginNewTransaction();
    REQUIRE (SiblingOrder::moveTo (knobs.getChild (0), 2, "knobs", undoManager).wasApplied());
    CHECK (orderOf (knobs) == "?231");

    undoManager.beginNewTransaction();
    REQUIRE (SiblingOrder::moveTo (knobs.getChild (3), 0, "knobs", undoManager).wasApplied());
    CHECK (orderOf (knobs) == "?123");

    undoManager.beginNewTransaction();
    REQUIRE (SiblingOrder::moveTo (knobs.getChild (3), 1, "knobs", undoManager).wasApplied());
    CHECK (orderOf (knobs) == "?132");

    REQUIRE (undoManager.undo());
    CHECK (orderOf (knobs) == "?123");
    REQUIRE (undoManager.undo());
    REQUIRE (undoManager.undo());
    CHECK (orderOf (knobs) == "1?23");
}

TEST_CASE ("SiblingOrder refuses a position that is not there and says how many there are", "[model][commands][SiblingOrder]")
{
    auto knobs = knobsWithAStranger();
    juce::UndoManager undoManager;

    for (const auto position : { -1, 3, 4 })
    {
        const auto result = SiblingOrder::moveTo (knobs.getChild (0), position, "knobs", undoManager);

        CHECK (! result.wasApplied());
        CHECK (result.refusal.contains ("3 knobs"));
    }

    CHECK (orderOf (knobs) == "1?23");
    CHECK (! undoManager.canUndo());
    CHECK (undoManager.getNumActionsInCurrentTransaction() == 0);
}

TEST_CASE ("SiblingOrder: a move to the position the node has is applied and leaves nothing to undo", "[model][commands][SiblingOrder]")
{
    auto knobs = knobsWithAStranger();
    juce::UndoManager undoManager;

    CHECK (SiblingOrder::moveTo (knobs.getChild (2), 1, "knobs", undoManager).wasApplied());
    CHECK (orderOf (knobs) == "1?23");
    CHECK (undoManager.getNumActionsInCurrentTransaction() == 0);
}
