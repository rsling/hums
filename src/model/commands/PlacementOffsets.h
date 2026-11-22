#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model::commands
{

// What every command that changes the loop length of a clip does to the
// clip's placements (docs/SCHEMA.md -> Loop-length changes): the loop length
// of an event clip, and the step length, step count, reset and skips of a
// step clip.
class PlacementOffsets
{
public:
    // Brings the offset of every Placement that references the clip, on a
    // track or on an automation lane, back into a loop of this length
    // (offset mod loop length), with the command's UndoManager, so that
    // invariant 4 holds in the same undo step. Slots have no offset.
    // `loopLength` is the clip's new loop length, at least 1.
    static void bringIntoLoop (juce::ValueTree project, const juce::String& clipId, std::int64_t loopLength, juce::UndoManager& undoManager);
};

} // namespace model::commands
