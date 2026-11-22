#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// What an AutomationLane requires of the clips it references
// (docs/SCHEMA.md -> Placements / Placement, invariant 9): an event clip
// that holds nothing but cc events — no note, no event of another kind,
// enabled or not, inside the loop or past it.
//
// The rule has two sides, and the commands hold both: a clip that does not
// meet it is not placed on a lane, and a clip that a lane references is
// given no content that would break it, whichever editor tries.
//
// And a clip on a lane is on lanes only [D21]: one that a lane references
// is not placed on a track and not put into a slot, and one that a track's
// placement or a slot references is not placed on a lane. Nothing is stored
// for this; a clip that nothing references yet may go either way.
//
// This is the one place that says what the rule is.
class AutomationClipRule
{
public:
    // Why no AutomationLane can reference this clip, in plain words; empty
    // when one can. The node must be a Clip node of a valid document.
    static juce::String whyNotForALane (const juce::ValueTree& clip);

    // True when a placement on any AutomationLane of the project references
    // the clip, so that it must stay as whyNotForALane wants it. `project`
    // is the Project node.
    static bool isOnALane (const juce::ValueTree& project, const juce::String& clipId);

    // True when a placement of a track itself, or a slot, references the
    // clip, so that no AutomationLane can.
    static bool isOnATrackOrInASlot (const juce::ValueTree& project, const juce::String& clipId);

    // What a command says when it refuses to place a clip of a lane on a
    // track or to put it into a slot, and the other way round.
    static juce::String refusalForAClipOfALane();
    static juce::String refusalForAClipOfATrack();
};

} // namespace model
