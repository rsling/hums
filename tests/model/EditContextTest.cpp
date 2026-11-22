// model::commands::EditContext: what a gesture carries to the commands that
// change a clip (docs/EDITING.md). What the refcount gate does with it is in
// RefcountGateTest.

#include <catch2/catch_test_macros.hpp>

#include "model/commands/EditContext.h"

#include <type_traits>

namespace
{

using model::Settings;
using model::commands::EditContext;

} // namespace

TEST_CASE ("An edit context holds the policy of its gesture and at first neither a reference nor a clone", "[model][commands][EditContext]")
{
    const EditContext updateAll (Settings::SharedEditPolicy::updateAll);
    const EditContext makeUnique (Settings::SharedEditPolicy::makeUnique);

    CHECK (updateAll.getPolicy() == Settings::SharedEditPolicy::updateAll);
    CHECK (makeUnique.getPolicy() == Settings::SharedEditPolicy::makeUnique);

    CHECK (! makeUnique.hasReferences());
    CHECK (makeUnique.getPlacementIds().empty());
    CHECK (makeUnique.getSlots().empty());
    CHECK (! makeUnique.hasClone());
    CHECK (makeUnique.getCloneId().isEmpty());
    CHECK (makeUnique.getOriginalId().isEmpty());
}

TEST_CASE ("An edit context holds its placements and slots in the order they were named, each once", "[model][commands][EditContext]")
{
    EditContext context (Settings::SharedEditPolicy::makeUnique);

    context.addPlacement ("b");
    context.addPlacement ("a");
    context.addPlacement ("b");
    context.addSlot ("track 1", 2);
    context.addSlot ("track 1", 0);
    context.addSlot ("track 2", 2);
    context.addSlot ("track 1", 2);

    CHECK (context.hasReferences());

    REQUIRE (context.getPlacementIds().size() == 2);
    CHECK (context.getPlacementIds()[0] == "b");
    CHECK (context.getPlacementIds()[1] == "a");

    REQUIRE (context.getSlots().size() == 3);
    CHECK (context.getSlots()[0].trackId == "track 1");
    CHECK (context.getSlots()[0].scene == 2);
    CHECK (context.getSlots()[1].trackId == "track 1");
    CHECK (context.getSlots()[1].scene == 0);
    CHECK (context.getSlots()[2].trackId == "track 2");
    CHECK (context.getSlots()[2].scene == 2);
}

TEST_CASE ("A slot alone is a reference too", "[model][commands][EditContext]")
{
    EditContext context (Settings::SharedEditPolicy::updateAll);
    context.addSlot ("track", 0);

    CHECK (context.hasReferences());
    CHECK (context.getPlacementIds().empty());
}

TEST_CASE ("An edit context remembers the clone of its gesture and the clip it was made from", "[model][commands][EditContext]")
{
    EditContext context (Settings::SharedEditPolicy::makeUnique);
    context.addPlacement ("a");

    context.rememberClone ("original", "clone");

    CHECK (context.hasClone());
    CHECK (context.getOriginalId() == "original");
    CHECK (context.getCloneId() == "clone");
}

TEST_CASE ("An edit context cannot be copied", "[model][commands][EditContext]")
{
    // Two copies would each make a clone of their own.
    CHECK (! std::is_copy_constructible_v<EditContext>);
    CHECK (! std::is_copy_assignable_v<EditContext>);
}

TEST_CASE ("An edit context belongs to an earlier gesture once the transaction it worked in is not the current one", "[model][commands][EditContext]")
{
    juce::ValueTree node ("Node");
    juce::UndoManager undoManager;

    undoManager.beginNewTransaction();
    EditContext context (Settings::SharedEditPolicy::makeUnique);
    CHECK (! context.belongsToAnEarlierGesture (undoManager));

    // The gesture does two things and the context is used after them.
    node.setProperty ("a", 1, &undoManager);
    node.setProperty ("b", 1, &undoManager);
    context.rememberUse (undoManager);
    CHECK (! context.belongsToAnEarlierGesture (undoManager));

    // More of the same gesture.
    node.setProperty ("c", 1, &undoManager);
    CHECK (! context.belongsToAnEarlierGesture (undoManager));

    SECTION ("a new transaction has begun")
    {
        undoManager.beginNewTransaction();
        CHECK (context.belongsToAnEarlierGesture (undoManager));

        // Also when the later gesture has done something by then, though
        // less; a context never sees its transaction shrink.
        node.setProperty ("d", 1, &undoManager);
        CHECK (context.belongsToAnEarlierGesture (undoManager));
    }

    SECTION ("the gesture was undone")
    {
        REQUIRE (undoManager.undo());
        CHECK (context.belongsToAnEarlierGesture (undoManager));
    }

    SECTION ("the gesture was undone and redone")
    {
        REQUIRE (undoManager.undo());
        REQUIRE (undoManager.redo());
        CHECK (context.belongsToAnEarlierGesture (undoManager));
    }
}

TEST_CASE ("A context whose gesture had left nothing to undo when it was last used cannot be told from a fresh one", "[model][commands][EditContext]")
{
    // The limit of the check, held here so that it is known: such a
    // context holds no clone.
    juce::UndoManager undoManager;

    undoManager.beginNewTransaction();
    EditContext context (Settings::SharedEditPolicy::makeUnique);
    context.rememberUse (undoManager);

    undoManager.beginNewTransaction();
    CHECK (! context.belongsToAnEarlierGesture (undoManager));
}
