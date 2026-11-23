// model::commands::CloneMark: what the refcount gate leaves in a gesture's
// undo transaction with a clone, and by which an edit context that has
// cloned knows that transaction (docs/EDITING.md).

#include <catch2/catch_test_macros.hpp>

#include "model/commands/CloneMark.h"

#include <juce_data_structures/juce_data_structures.h>

namespace
{

using model::commands::CloneMark;

const char* const cloneId = "1a2b3c4d5e6f40718293a4b5c6d7e8f9";
const char* const otherCloneId = "9f8e7d6c5b4a40718293a4b5c6d7e8f9";

} // namespace

TEST_CASE ("A clone mark is in the transaction it was left in while that is the current one", "[model][commands][CloneMark]")
{
    juce::ValueTree node ("Node");
    juce::UndoManager undoManager;

    undoManager.beginNewTransaction();
    CHECK (! CloneMark::isInCurrentTransaction (undoManager, cloneId));

    node.setProperty ("a", 1, &undoManager);
    CloneMark::leaveIn (undoManager, cloneId);
    CHECK (CloneMark::isInCurrentTransaction (undoManager, cloneId));

    // Whatever the transaction goes on to hold.
    node.setProperty ("b", 1, &undoManager);
    node.setProperty ("c", 1, &undoManager);
    CHECK (CloneMark::isInCurrentTransaction (undoManager, cloneId));

    // The mark of one clone is not that of another.
    CHECK (! CloneMark::isInCurrentTransaction (undoManager, otherCloneId));

    SECTION ("not once a new transaction has begun, whatever that one holds")
    {
        undoManager.beginNewTransaction();
        CHECK (! CloneMark::isInCurrentTransaction (undoManager, cloneId));

        for (int i = 0; i < 8; ++i)
        {
            node.setProperty ("d", i, &undoManager);
            node.setProperty ("e", i, &undoManager);
            CHECK (! CloneMark::isInCurrentTransaction (undoManager, cloneId));
        }

        // Nor when that one holds the mark of another clone.
        CloneMark::leaveIn (undoManager, otherCloneId);
        CHECK (! CloneMark::isInCurrentTransaction (undoManager, cloneId));
        CHECK (CloneMark::isInCurrentTransaction (undoManager, otherCloneId));
    }

    SECTION ("not once the transaction was undone")
    {
        REQUIRE (undoManager.undo());
        CHECK (! CloneMark::isInCurrentTransaction (undoManager, cloneId));
    }

    SECTION ("not once it was undone and redone: redo begins a transaction of its own")
    {
        REQUIRE (undoManager.undo());
        REQUIRE (undoManager.redo());
        CHECK (! CloneMark::isInCurrentTransaction (undoManager, cloneId));

        node.setProperty ("d", 1, &undoManager);
        CHECK (! CloneMark::isInCurrentTransaction (undoManager, cloneId));
    }
}

TEST_CASE ("A clone mark changes nothing and is undone and redone with its transaction", "[model][commands][CloneMark]")
{
    juce::ValueTree node ("Node");
    juce::UndoManager undoManager;

    undoManager.beginNewTransaction();
    node.setProperty ("a", 1, &undoManager);
    const auto before = node.toXmlString();

    CloneMark::leaveIn (undoManager, cloneId);
    CHECK (node.toXmlString() == before);
    CHECK (undoManager.getNumActionsInCurrentTransaction() == 2);

    REQUIRE (undoManager.undo());
    CHECK (! node.hasProperty ("a"));
    CHECK (! undoManager.canUndo());

    REQUIRE (undoManager.redo());
    CHECK (node.toXmlString() == before);
}

TEST_CASE ("A clone mark can be the first thing a transaction holds", "[model][commands][CloneMark]")
{
    juce::UndoManager undoManager;

    undoManager.beginNewTransaction();
    CloneMark::leaveIn (undoManager, cloneId);

    CHECK (CloneMark::isInCurrentTransaction (undoManager, cloneId));
    CHECK (undoManager.canUndo());
}
