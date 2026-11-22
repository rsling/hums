#include "model/commands/PlacementOffsets.h"

#include "model/Ids.h"
#include "model/PlacementModel.h"
#include "model/commands/NodeLookup.h"

namespace model::commands
{

void PlacementOffsets::bringIntoLoop (juce::ValueTree project, const juce::String& clipId, std::int64_t loopLength, juce::UndoManager& undoManager)
{
    jassert (loopLength >= 1);

    for (const auto& track : NodeLookup::childrenOfType (project.getChildWithName (ids::Tracks), ids::Track))
    {
        auto parents = NodeLookup::childrenOfType (track.getChildWithName (ids::AutomationLanes), ids::AutomationLane);
        parents.push_back (track.getChildWithName (ids::Placements));

        for (const auto& parent : parents)
        {
            for (auto node : NodeLookup::childrenOfType (parent, ids::Placement))
            {
                const PlacementModel placement (node);

                if (placement.getClipId() != clipId)
                    continue;

                // A valid document has no negative offset.
                const auto offset = placement.getOffset();
                const auto offsetInLoop = offset % loopLength;

                if (offsetInLoop == offset)
                    continue;

                // An offset of 0 is the default and is not stored, as on a
                // new placement. The offset is the last property of a
                // placement in every document Hums has written, so undo,
                // which puts a removed property back behind the others,
                // puts it where it stood.
                if (offsetInLoop == 0)
                    node.removeProperty (ids::offset, &undoManager);
                else
                    node.setProperty (ids::offset, static_cast<juce::int64> (offsetInLoop), &undoManager);
            }
        }
    }
}

} // namespace model::commands
