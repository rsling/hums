#pragma once

#include "model/EventModel.h"
#include "model/commands/CommandResult.h"
#include "model/commands/EditContext.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model::commands
{

// The commands for what an event clip holds (docs/SCHEMA.md -> ClipPool /
// Clip, -> Event clip: Events): its notes, its other events, its loop length
// and its quantise setting. A step clip has none of these, and every command
// here refuses one.
//
// Each is a change to a clip, so it passes the refcount gate with the
// gesture's edit context (docs/EDITING.md, RefcountGate): the change is made
// for every reference of the clip, or, under "make unique" through the
// references the context names, to a clone that only those reference. A
// change that changes nothing makes no clone.
//
// A note or an event is named by its position among the clip's notes, or
// among its events, in child order, from 0. A node handle would not do: under
// "make unique" the change lands on the clone, where a node of the original
// is no node at all, and the position is the same in both. Removing one moves
// the positions of those behind it.
//
// A clip that an AutomationLane references holds nothing but cc events
// (invariant 9, model/AutomationClipRule), so a note or an event of another
// kind is refused there. What counts is the clip the change lands on: a clone
// is on a lane when one of the context's references is a lane's placement.
//
// Like every command: `project` is the Project node of a valid document; a
// command checks everything before it touches the tree, applies completely
// or refuses and changes nothing, and never begins a transaction.

// What a note is made of (docs/SCHEMA.md -> Note). As made here, it is the
// note the schema describes when only its place and pitch are stored.
struct NoteValues
{
    // The start, relative to the clip start; 0 or later. It may lie at or
    // past the loop length: such a note is kept and never played.
    std::int64_t tick = 0;
    // At least one tick, and the note must not end past the largest tick.
    std::int64_t length = 0;
    // 0 to 127.
    int pitch = 60;
    // 1 to 127.
    int velocity = 100;
    // 0 to 127.
    int offVelocity = 64;
    bool enabled = true;
};

// What an event that is no note is made of (docs/SCHEMA.md -> Event).
struct EventValues
{
    // 0 or later.
    std::int64_t tick = 0;
    EventModel::Kind kind = EventModel::Kind::cc;
    // What a and b mean depends on the kind (EventModel::Kind), and so do
    // their ranges. One that the kind does not use is 0.
    int a = 0;
    int b = 0;
    // The payload of a sysex event as hexadecimal digits, two to a byte,
    // without F0 and F7, at most 64 KB; empty for every other kind.
    juce::String data;
    bool enabled = true;
};

// Puts a note behind the clip's other content, so that it comes last among
// the notes at its tick. Refused when the values are not a note's, when the
// clip is a step clip, and when the clip the change lands on is on an
// AutomationLane.
CommandResult addNote (juce::ValueTree project, const juce::String& clipId, const NoteValues& values, EditContext& editContext, juce::UndoManager& undoManager);

// Gives the note at that position these values; it keeps its place among
// the others. Refused when there is no such note or the values are not a
// note's.
CommandResult setNote (juce::ValueTree project, const juce::String& clipId, int noteIndex, const NoteValues& values, EditContext& editContext, juce::UndoManager& undoManager);

CommandResult removeNote (juce::ValueTree project, const juce::String& clipId, int noteIndex, EditContext& editContext, juce::UndoManager& undoManager);

// Puts an event behind the clip's other content, so that it comes last
// among the events at its tick. Refused when the values are not those of an
// event of its kind, when the clip is a step clip, and, for every kind but
// cc, when the clip the change lands on is on an AutomationLane.
CommandResult addEvent (juce::ValueTree project, const juce::String& clipId, const EventValues& values, EditContext& editContext, juce::UndoManager& undoManager);

// Gives the event at that position these values, its kind included; it
// keeps its place among the others. Refused like addEvent, and when there is
// no such event.
CommandResult setEvent (juce::ValueTree project, const juce::String& clipId, int eventIndex, const EventValues& values, EditContext& editContext, juce::UndoManager& undoManager);

CommandResult removeEvent (juce::ValueTree project, const juce::String& clipId, int eventIndex, EditContext& editContext, juce::UndoManager& undoManager);

// The length of one pass of the loop, at least one tick. Content is neither
// moved nor removed; what now lies at or past the loop length is kept and not
// played. Every placement that references the changed clip gets its offset
// brought back into the new loop (offset mod loop length) in the same undo
// step, so that invariant 4 holds.
CommandResult setClipLoopLength (juce::ValueTree project, const juce::String& clipId, std::int64_t loopLength, EditContext& editContext, juce::UndoManager& undoManager);

// The playback quantisation of note onsets: "" (off), "1/2", "1/4", "1/8",
// "1/2T", "1/4T" or "1/8T"; anything else is refused. The stored ticks stay
// as they are.
CommandResult setClipQuantise (juce::ValueTree project, const juce::String& clipId, const juce::String& quantise, EditContext& editContext, juce::UndoManager& undoManager);

} // namespace model::commands
