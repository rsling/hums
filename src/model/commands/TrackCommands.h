#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// The commands for the tracks and their knob assignments (docs/SCHEMA.md ->
// Tracks / Track, Knobs / Knob). What a track holds besides — placements,
// automation lanes, slots — has commands of its own.
//
// A track is named by its id. A knob is named by its track and its
// controller number, which is unique within the track. A position counts
// from 0: among the tracks from top to bottom, among a track's knobs from
// left to right.
//
// Like every command: `project` is the Project node of a valid document; a
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.

// Makes a Track with a new id below the others: unassigned, channel 1, not
// recordable, neither muted nor soloed, with its four empty containers. The
// result carries the id. A name may be any text, also an empty one and one
// another track already has.
CommandResult addTrack (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager);

// Removes the Track with everything it owns: its placements, automation
// lanes, knobs and slots. The clips they referenced stay in the pool, with a
// smaller refcount. Refused when no Track has this id.
CommandResult removeTrack (juce::ValueTree project, const juce::String& trackId, juce::UndoManager& undoManager);

// Moves the Track to that position among the tracks; the others keep their
// order. Refused when the position is below 0 or not below the number of
// tracks.
CommandResult moveTrack (juce::ValueTree project, const juce::String& trackId, int toPosition, juce::UndoManager& undoManager);

CommandResult setTrackName (juce::ValueTree project, const juce::String& trackId, const juce::String& name, juce::UndoManager& undoManager);

// "#AARRGGBB", or empty for no colour; anything else is refused.
CommandResult setTrackColour (juce::ValueTree project, const juce::String& trackId, const juce::String& colour, juce::UndoManager& undoManager);

// Which Player the track plays through; empty = unassigned (the track still
// renders and exports). Refused when the id is not empty and no Player has
// it.
CommandResult setTrackPlayer (juce::ValueTree project, const juce::String& trackId, const juce::String& playerId, juce::UndoManager& undoManager);

// 1 to 16; anything else is refused.
CommandResult setTrackChannel (juce::ValueTree project, const juce::String& trackId, int channel, juce::UndoManager& undoManager);

// Which Performer the track records from; empty = not recordable. Refused
// when the id is not empty and no Performer has it.
CommandResult setTrackPerformer (juce::ValueTree project, const juce::String& trackId, const juce::String& performerId, juce::UndoManager& undoManager);

// Mute and solo are document state and undoable. They reach the engine as
// live commands from a listener on the document, never from here
// (docs/ARCHITECTURE.md -> Threads).
CommandResult setTrackMute (juce::ValueTree project, const juce::String& trackId, bool mute, juce::UndoManager& undoManager);

CommandResult setTrackSolo (juce::ValueTree project, const juce::String& trackId, bool solo, juce::UndoManager& undoManager);

// Adds a knob to the right of the track's others. Refused when the
// controller number is outside 0 to 127 or the track already has a knob for
// it.
CommandResult addKnob (juce::ValueTree project, const juce::String& trackId, int cc, const juce::String& label, juce::UndoManager& undoManager);

// Refused when the track has no knob for this controller number.
CommandResult removeKnob (juce::ValueTree project, const juce::String& trackId, int cc, juce::UndoManager& undoManager);

// Moves the knob to that position among the track's knobs. Refused when the
// position is below 0 or not below the number of knobs.
CommandResult moveKnob (juce::ValueTree project, const juce::String& trackId, int cc, int toPosition, juce::UndoManager& undoManager);

// Gives the knob another controller number; its place and label stay.
// Nothing that was recorded with the old number is rewritten. Refused when
// the new number is outside 0 to 127 or is another knob's on this track.
CommandResult setKnobCc (juce::ValueTree project, const juce::String& trackId, int cc, int newCc, juce::UndoManager& undoManager);

CommandResult setKnobLabel (juce::ValueTree project, const juce::String& trackId, int cc, const juce::String& label, juce::UndoManager& undoManager);

} // namespace model::commands
