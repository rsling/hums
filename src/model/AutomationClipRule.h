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
// given no content that would break it, whichever editor tries. This is the
// one place that says what the rule is.
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
};

} // namespace model
