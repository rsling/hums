// model::commands for the pad map (docs/SCHEMA.md -> Routing, PadMap / Pad).

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/PadMapModel.h"
#include "model/commands/PadMapCommands.h"

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;

std::vector<model::PadModel> padsOf (const CommandFixture& fixture)
{
    return model::PadMapModel (fixture.routing().getChildWithName (ids::PadMap)).getPads();
}

} // namespace

TEST_CASE ("addPad adds a pad, and undo takes it away again", "[model][commands][PadMapCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    // Two pads may produce the same pitch: 42 is pad 1's already.
    REQUIRE (commands::addPad (fixture.project, 7, 38, 42, "Open hat", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto pads = padsOf (fixture);
    REQUIRE (pads.size() == 3);
    CHECK (pads[2].getIndex() == 7);
    CHECK (pads[2].getInputPitch() == 38);
    CHECK (pads[2].getPitch() == 42);
    CHECK (pads[2].getLabel() == "Open hat");

    const auto after = fixture.xml();
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
}

TEST_CASE ("addPad takes the ends of every range", "[model][commands][PadMapCommands]")
{
    CommandFixture fixture;

    CHECK (commands::addPad (fixture.project, 2, 0, 127, "", fixture.undoManager).wasApplied());
    CHECK (commands::addPad (fixture.project, 3, 127, 0, "", fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("addPad refuses a pad the map cannot hold", "[model][commands][PadMapCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::addPad (fixture.project, -1, 40, 40, "", fixture.undoManager).wasApplied());  // no pad number
    CHECK (! commands::addPad (fixture.project, 1, 40, 40, "", fixture.undoManager).wasApplied());   // pad 1 is there
    CHECK (! commands::addPad (fixture.project, 2, 36, 40, "", fixture.undoManager).wasApplied());   // pad 0 sends 36
    CHECK (! commands::addPad (fixture.project, 2, -1, 40, "", fixture.undoManager).wasApplied());
    CHECK (! commands::addPad (fixture.project, 2, 128, 40, "", fixture.undoManager).wasApplied());
    CHECK (! commands::addPad (fixture.project, 2, 40, -1, "", fixture.undoManager).wasApplied());
    CHECK (! commands::addPad (fixture.project, 2, 40, 128, "", fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("removePad removes the pad with that index and no other", "[model][commands][PadMapCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    REQUIRE (commands::removePad (fixture.project, 0, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (padsOf (fixture).size() == 1);
    CHECK (padsOf (fixture)[0].getIndex() == 1);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("A pad is remapped through commands", "[model][commands][PadMapCommands]")
{
    CommandFixture fixture;

    REQUIRE (commands::setPadInputPitch (fixture.project, 1, 60, fixture.undoManager).wasApplied());
    REQUIRE (commands::setPadPitch (fixture.project, 1, 36, fixture.undoManager).wasApplied());
    REQUIRE (commands::setPadLabel (fixture.project, 1, "Second kick", fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());

    const auto pad = padsOf (fixture)[1];
    CHECK (pad.getInputPitch() == 60);
    CHECK (pad.getPitch() == 36);
    CHECK (pad.getLabel() == "Second kick");

    // The note pad 1 no longer sends is free for another pad.
    CHECK (commands::setPadInputPitch (fixture.project, 0, 37, fixture.undoManager).wasApplied());
    CHECK (fixture.isValid());
}

TEST_CASE ("Setting a pad's input pitch to the one it has is applied and leaves nothing to undo", "[model][commands][PadMapCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (commands::setPadInputPitch (fixture.project, 0, 36, fixture.undoManager).wasApplied());
    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A pad command that would break the map is refused and changes nothing", "[model][commands][PadMapCommands]")
{
    CommandFixture fixture;
    const auto before = fixture.xml();

    CHECK (! commands::setPadInputPitch (fixture.project, 1, 36, fixture.undoManager).wasApplied()); // pad 0 sends 36
    CHECK (! commands::setPadInputPitch (fixture.project, 1, 128, fixture.undoManager).wasApplied());
    CHECK (! commands::setPadInputPitch (fixture.project, 1, -1, fixture.undoManager).wasApplied());
    CHECK (! commands::setPadPitch (fixture.project, 1, 128, fixture.undoManager).wasApplied());
    CHECK (! commands::setPadPitch (fixture.project, 1, -1, fixture.undoManager).wasApplied());

    // No pad 5.
    CHECK (! commands::removePad (fixture.project, 5, fixture.undoManager).wasApplied());
    CHECK (! commands::setPadInputPitch (fixture.project, 5, 60, fixture.undoManager).wasApplied());
    CHECK (! commands::setPadPitch (fixture.project, 5, 60, fixture.undoManager).wasApplied());
    CHECK (! commands::setPadLabel (fixture.project, 5, "x", fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
}
