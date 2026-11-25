#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model::commands
{

// The commands for the Timeline (docs/SCHEMA.md -> Timeline): the tempo map,
// the time-signature map and the markers.
//
// A TempoEvent and a TimeSigEvent are named by their tick, of which the
// Timeline holds at most one each (invariant 6). A Marker has no id and may
// share its tick with another, so it is named by its node, as a selection
// holds it.
//
// Like every command: `project` is the Project node of a valid document; a
// command applies completely or refuses and changes nothing, and never
// begins a transaction.

// Sets the tempo from this tick on: changes the TempoEvent at the tick, or
// adds one when there is none. The tempo is stored with six significant
// digits, three decimals up to 999.999 [D14] (model/CanonicalDouble).
// Refused when the tick is negative and when the tempo, as it would be
// stored, is not one of 3.57628 to 999.999 bpm [D23] (model/TempoRange).
CommandResult setTempo (juce::ValueTree project, std::int64_t tick, double bpm, juce::UndoManager& undoManager);

// Refused at tick 0, where a Timeline always has a tempo, and when there is
// no TempoEvent at the tick.
CommandResult removeTempoEvent (juce::ValueTree project, std::int64_t tick, juce::UndoManager& undoManager);

// Sets the time signature from this tick on: changes the TimeSigEvent at the
// tick, or adds one when there is none. Refused when the tick is negative,
// the numerator is outside 1 to 32, or the denominator is not one of 1, 2,
// 4, 8, 16, 32, 64.
CommandResult setTimeSignature (juce::ValueTree project, std::int64_t tick, int numerator, int denominator, juce::UndoManager& undoManager);

// Refused at tick 0, where a Timeline always has a time signature, and when
// there is no TimeSigEvent at the tick.
CommandResult removeTimeSigEvent (juce::ValueTree project, std::int64_t tick, juce::UndoManager& undoManager);

// Refused when the tick is negative.
CommandResult addMarker (juce::ValueTree project, std::int64_t tick, const juce::String& name, juce::UndoManager& undoManager);

// The three below are refused when `marker` is not a Marker of this
// project's Timeline.
CommandResult removeMarker (juce::ValueTree project, const juce::ValueTree& marker, juce::UndoManager& undoManager);

// Refused when the tick is negative.
CommandResult moveMarker (juce::ValueTree project, const juce::ValueTree& marker, std::int64_t tick, juce::UndoManager& undoManager);

CommandResult renameMarker (juce::ValueTree project, const juce::ValueTree& marker, const juce::String& name, juce::UndoManager& undoManager);

} // namespace model::commands
