#include "model/commands/SlotCommands.h"

#include "model/Ids.h"
#include "model/PropertyReader.h"
#include "model/commands/NodeLookup.h"

#include <functional>
#include <limits>
#include <vector>

namespace model::commands
{

namespace
{

    // What a renumbering answers for a row that is to go.
    const int noRow = -1;

    const int largestRow = std::numeric_limits<int>::max();

    juce::ValueTree trackOf (const juce::ValueTree& project, const juce::String& trackId)
    {
        jassert (project.hasType (ids::Project));
        return NodeLookup::childWithId (project.getChildWithName (ids::Tracks), ids::Track, trackId);
    }

    juce::ValueTree slotOf (const juce::ValueTree& track, int scene)
    {
        return NodeLookup::childWithNumber (track.getChildWithName (ids::Slots), ids::Slot, ids::scene, scene);
    }

    CommandResult noSuchTrack (const juce::String& trackId)
    {
        return CommandResult::refused ("There is no Track with the id \"" + trackId + "\".");
    }

    CommandResult notARow (int scene)
    {
        return CommandResult::refused ("A scene is row 0 or later, not " + juce::String (scene) + ".");
    }

    // A node that stands in a row of the session grid, and the property
    // that says in which: a Slot with its `scene`, a Scene with its `index`.
    struct NodeInARow
    {
        juce::ValueTree node;
        juce::Identifier rowProperty;
        int row = 0;
    };

    // Every Slot of every track and every Scene of the Session.
    std::vector<NodeInARow> nodesInRows (const juce::ValueTree& project)
    {
        jassert (project.hasType (ids::Project));

        std::vector<NodeInARow> found;

        for (const auto& track : NodeLookup::childrenOfType (project.getChildWithName (ids::Tracks), ids::Track))
            for (const auto& slot : NodeLookup::childrenOfType (track.getChildWithName (ids::Slots), ids::Slot))
                found.push_back ({ slot, ids::scene, PropertyReader::getRequiredInt (slot, ids::scene) });

        for (const auto& scene : NodeLookup::childrenOfType (project.getChildWithName (ids::Session), ids::Scene))
            found.push_back ({ scene, ids::index, PropertyReader::getRequiredInt (scene, ids::index) });

        return found;
    }

    // Gives every Slot and every Scene the row that `rowAfter` answers for
    // the row it is in, and takes out those for which it answers noRow. The
    // answers for two rows that stay must differ, so that rows that were
    // apart are apart afterwards; on the way there two nodes may share a
    // row for a moment, which nobody sees inside one command.
    void renumber (const juce::ValueTree& project, const std::function<int (int)>& rowAfter, juce::UndoManager& undoManager)
    {
        for (auto& inARow : nodesInRows (project))
        {
            const auto after = rowAfter (inARow.row);

            if (after == noRow)
                inARow.node.getParent().removeChild (inARow.node, &undoManager);
            else if (after != inARow.row)
                inARow.node.setProperty (inARow.rowProperty, after, &undoManager);
        }
    }

} // namespace

CommandResult setSlot (juce::ValueTree project, const juce::String& trackId, int scene, const juce::String& clipId, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    if (scene < 0)
        return notARow (scene);

    if (! NodeLookup::childWithId (project.getChildWithName (ids::ClipPool), ids::Clip, clipId).isValid())
        return CommandResult::refused ("There is no Clip with the id \"" + clipId + "\".");

    auto slot = slotOf (track, scene);

    if (slot.isValid())
    {
        slot.setProperty (ids::clipId, clipId, &undoManager);
        return CommandResult::applied();
    }

    // Built apart and added whole, so that undo knows one action.
    juce::ValueTree added (ids::Slot);
    added.setProperty (ids::scene, scene, nullptr);
    added.setProperty (ids::clipId, clipId, nullptr);

    track.getChildWithName (ids::Slots).appendChild (added, &undoManager);

    return CommandResult::applied();
}

CommandResult clearSlot (juce::ValueTree project, const juce::String& trackId, int scene, juce::UndoManager& undoManager)
{
    const auto track = trackOf (project, trackId);

    if (! track.isValid())
        return noSuchTrack (trackId);

    const auto slot = slotOf (track, scene);

    if (! slot.isValid())
        return CommandResult::refused ("The track's slot in scene " + juce::String (scene) + " is empty.");

    slot.getParent().removeChild (slot, &undoManager);

    return CommandResult::applied();
}

CommandResult insertScene (juce::ValueTree project, int atScene, juce::UndoManager& undoManager)
{
    if (atScene < 0)
        return notARow (atScene);

    for (const auto& inARow : nodesInRows (project))
        if (inARow.row == largestRow)
            return CommandResult::refused ("Row " + juce::String (largestRow) + " is in use, and there is no row after it to which it could move.");

    renumber (
        project,
        [atScene] (int row) { return row >= atScene ? row + 1 : row; },
        undoManager);

    return CommandResult::applied();
}

CommandResult removeScene (juce::ValueTree project, int scene, juce::UndoManager& undoManager)
{
    if (scene < 0)
        return notARow (scene);

    renumber (
        project,
        [scene] (int row) {
            if (row == scene)
                return noRow;

            return row > scene ? row - 1 : row;
        },
        undoManager);

    return CommandResult::applied();
}

CommandResult moveScene (juce::ValueTree project, int fromScene, int toScene, juce::UndoManager& undoManager)
{
    if (fromScene < 0)
        return notARow (fromScene);

    if (toScene < 0)
        return notARow (toScene);

    renumber (
        project,
        [fromScene, toScene] (int row) {
            if (row == fromScene)
                return toScene;

            // Downwards: the rows it passes move up into the gap it left.
            if (fromScene < row && row <= toScene)
                return row - 1;

            // Upwards: the rows it passes move down.
            if (toScene <= row && row < fromScene)
                return row + 1;

            return row;
        },
        undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
