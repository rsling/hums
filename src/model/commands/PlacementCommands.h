#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model::commands
{

// The commands for placements (docs/SCHEMA.md -> Placements / Placement,
// docs/EDITING.md): putting a clip on a track, moving a placement, resizing
// and splitting it, and removing it.
//
// A placement is named by its id, which is unique in the project. A
// placement on an AutomationLane is the same kind of node as a track's own,
// and every command here that names a placement takes either: moving it in
// time, resizing, splitting and removing it leave the clip it references
// alone, so none of them can break what a lane requires of its clips. Only
// putting a placement on a lane, and the lanes themselves, have commands of
// their own.
//
// None of these commands changes a clip, so none needs an edit context: the
// pool is untouched by all of them.
//
// A placement covers the ticks from its start up to, not including, its
// start + length. Two placements of one track, or of one lane, never share a
// tick; one may begin on the tick at which another ends. A placement whose
// end would lie past the largest tick there is, is refused.
//
// Like every command: `project` is the Project node of a valid document; a
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.

// Puts the clip on the track, as a placement of its own with a new id. It
// plays the clip's loop from its beginning (offset 0), again and again for
// its whole length. The result carries the id. A clip of either kind may be
// placed, any number of times. Refused when the track or the clip is not
// there, the start is negative, the length is not at least one tick, or the
// track already has a placement in those ticks.
CommandResult addPlacement (juce::ValueTree project, const juce::String& trackId, const juce::String& clipId, std::int64_t start, std::int64_t length, juce::UndoManager& undoManager);

// Moves the placement along the timeline, on the track or lane it is on. Its
// length and its offset stay. Refused when the new start is negative or
// another placement of the same track or lane is in the way.
CommandResult movePlacement (juce::ValueTree project, const juce::String& placementId, std::int64_t toStart, juce::UndoManager& undoManager);

// Moves one of a track's own placements to a track, which may be the one it
// is on, and to a start there. It stays the same placement: its id, clip,
// length and offset stay. Refused for a placement on an AutomationLane, when
// the track is not there, the start is negative, or another placement of
// that track is in the way.
CommandResult movePlacementToTrack (juce::ValueTree project, const juce::String& placementId, const juce::String& toTrackId, std::int64_t toStart, juce::UndoManager& undoManager);

// Resize: gives the placement another length. Its start and its offset stay,
// so a longer placement repeats more of the loop and a shorter one plays
// less of it; nothing is stretched. Refused when the length is not at least
// one tick or the placement would reach into the next one.
CommandResult resizePlacement (juce::ValueTree project, const juce::String& placementId, std::int64_t length, juce::UndoManager& undoManager);

// Splits the placement at an arrangement tick into two of the same clip. The
// one that was there keeps its id and ends at the tick. The new one begins
// there, stands right behind it in the document, and goes on at the point of
// the loop the first has reached: its offset is
// (offset + (tick - start)) mod loop length. Playback is as before. The
// result carries the id of the new placement. Refused unless the tick lies
// inside the placement, after its start and before its end.
CommandResult splitPlacement (juce::ValueTree project, const juce::String& placementId, std::int64_t atTick, juce::UndoManager& undoManager);

// Removes the placement. Its clip stays in the pool, with a smaller
// refcount.
CommandResult removePlacement (juce::ValueTree project, const juce::String& placementId, juce::UndoManager& undoManager);

} // namespace model::commands
