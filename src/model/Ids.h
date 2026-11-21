#pragma once

#include <juce_data_structures/juce_data_structures.h>

// Every ValueTree type and property identifier of the document model, and
// the only place where one is spelled as a string (guardrail 1,
// tools/check_identifiers.py). This file mirrors docs/SCHEMA.md exactly; if
// the two disagree, fix one and bump schemaVersion.
//
// The form of a declaration is fixed, because the guardrail script reads this
// file: one identifier per line, the constant named exactly like its string.
//
//     inline const juce::Identifier pitch { "pitch" };
//
// A name is declared once, however many node types use it.
namespace model::ids
{

//==============================================================================
// Node types, in the order of the tree in docs/SCHEMA.md.

inline const juce::Identifier Project { "Project" };

inline const juce::Identifier Routing { "Routing" };
inline const juce::Identifier Performer { "Performer" };
inline const juce::Identifier Player { "Player" };
inline const juce::Identifier PadMap { "PadMap" };
inline const juce::Identifier Pad { "Pad" };

inline const juce::Identifier Timeline { "Timeline" };
inline const juce::Identifier TempoEvent { "TempoEvent" };
inline const juce::Identifier TimeSigEvent { "TimeSigEvent" };
inline const juce::Identifier Marker { "Marker" };

inline const juce::Identifier ClipPool { "ClipPool" };
inline const juce::Identifier Clip { "Clip" };
inline const juce::Identifier Events { "Events" };
inline const juce::Identifier Note { "Note" };
inline const juce::Identifier Event { "Event" };
inline const juce::Identifier Grid { "Grid" };
inline const juce::Identifier Lane { "Lane" };
inline const juce::Identifier Cell { "Cell" };
inline const juce::Identifier CcValue { "CcValue" };

inline const juce::Identifier Tracks { "Tracks" };
inline const juce::Identifier Track { "Track" };
inline const juce::Identifier Placements { "Placements" };
inline const juce::Identifier Placement { "Placement" };
inline const juce::Identifier AutomationLanes { "AutomationLanes" };
inline const juce::Identifier AutomationLane { "AutomationLane" };
inline const juce::Identifier Knobs { "Knobs" };
inline const juce::Identifier Knob { "Knob" };
inline const juce::Identifier Slots { "Slots" };
inline const juce::Identifier Slot { "Slot" };

inline const juce::Identifier Session { "Session" };
inline const juce::Identifier Scene { "Scene" };

inline const juce::Identifier UiState { "UiState" };

//==============================================================================
// Properties. The canonical writer emits a node's properties in the order
// they are declared here (docs/SCHEMA.md -> Canonical form), so this is one
// order for all node types, arranged so that every node's properties come
// out in the order of its table in docs/SCHEMA.md. Moving a line changes the
// bytes of every saved project. The comment names the node types that carry
// the property.

inline const juce::Identifier schemaVersion { "schemaVersion" }; // Project
inline const juce::Identifier id { "id" }; // Performer, Player, Clip, Lane, Track, Placement, AutomationLane
inline const juce::Identifier index { "index" }; // Pad, Scene
inline const juce::Identifier inputPitch { "inputPitch" }; // Pad
inline const juce::Identifier tick { "tick" }; // TempoEvent, TimeSigEvent, Marker, Note, Event
inline const juce::Identifier name { "name" }; // Project, Performer, Player, Marker, Clip, Lane, Track, AutomationLane, Scene
inline const juce::Identifier ppq { "ppq" }; // Project
inline const juce::Identifier deviceIdentifier { "deviceIdentifier" }; // Performer, Player
inline const juce::Identifier deviceName { "deviceName" }; // Performer, Player
inline const juce::Identifier channelFilter { "channelFilter" }; // Performer
inline const juce::Identifier sendClock { "sendClock" }; // Player
inline const juce::Identifier bpm { "bpm" }; // TempoEvent
inline const juce::Identifier numerator { "numerator" }; // TimeSigEvent
inline const juce::Identifier denominator { "denominator" }; // TimeSigEvent
inline const juce::Identifier kind { "kind" }; // Clip, Event
inline const juce::Identifier loopLength { "loopLength" }; // Clip
inline const juce::Identifier quantise { "quantise" }; // Clip
inline const juce::Identifier colour { "colour" }; // Clip, Track
inline const juce::Identifier a { "a" }; // Event
inline const juce::Identifier b { "b" }; // Event
inline const juce::Identifier data { "data" }; // Event
inline const juce::Identifier stepLength { "stepLength" }; // Grid
inline const juce::Identifier stepCount { "stepCount" }; // Grid
inline const juce::Identifier resetAt { "resetAt" }; // Grid
inline const juce::Identifier skipMask { "skipMask" }; // Grid
inline const juce::Identifier fixedPitch { "fixedPitch" }; // Lane
inline const juce::Identifier cc { "cc" }; // CcValue, Knob
inline const juce::Identifier value { "value" }; // CcValue
inline const juce::Identifier playerId { "playerId" }; // Track
inline const juce::Identifier channel { "channel" }; // Track
inline const juce::Identifier performerId { "performerId" }; // Track
inline const juce::Identifier mute { "mute" }; // Track
inline const juce::Identifier solo { "solo" }; // Track
inline const juce::Identifier scene { "scene" }; // Slot
inline const juce::Identifier clipId { "clipId" }; // Placement, Slot
inline const juce::Identifier start { "start" }; // Placement
inline const juce::Identifier length { "length" }; // Note, Placement
inline const juce::Identifier pitch { "pitch" }; // Pad, Note, Cell
inline const juce::Identifier label { "label" }; // Pad, Knob
inline const juce::Identifier on { "on" }; // Cell
inline const juce::Identifier velocity { "velocity" }; // Note, Cell
inline const juce::Identifier offVelocity { "offVelocity" }; // Note
inline const juce::Identifier enabled { "enabled" }; // Note, Event
inline const juce::Identifier gate { "gate" }; // Cell
inline const juce::Identifier offset { "offset" }; // Placement
inline const juce::Identifier launchQuantise { "launchQuantise" }; // Session
inline const juce::Identifier activeTab { "activeTab" }; // UiState
inline const juce::Identifier sharedEditPolicy { "sharedEditPolicy" }; // UiState
inline const juce::Identifier loopStart { "loopStart" }; // UiState
inline const juce::Identifier loopEnd { "loopEnd" }; // UiState
inline const juce::Identifier loopEnabled { "loopEnabled" }; // UiState
inline const juce::Identifier exportLength { "exportLength" }; // UiState

//==============================================================================
// VIEW STATE — not document model.
//
// Each view may keep one child node under UiState, named after itself, for
// zoom, scroll, visible CC rows and the like (docs/SCHEMA.md -> UiState). Its
// type and property identifiers are declared below this line and nowhere
// else. The Validator does not look inside these nodes, and adding one here
// is not a schema bump.
//
// None yet: the first view arrives with milestone 4.

} // namespace model::ids
