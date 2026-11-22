#pragma once

#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// How the commands move a node whose place among its siblings means
// something (docs/SCHEMA.md: Tracks from top to bottom, Knobs from left to
// right, AutomationLanes from the overridden to the overriding).
class SiblingOrder
{
public:
    // Moves the child to a position among its siblings of the same type;
    // the others keep their order. The position counts from 0 and among
    // those only, so a child of another type (data from a newer build)
    // neither counts nor moves. Refused when the position is below 0 or not
    // below their number; `what` names the siblings in the refusal
    // ("tracks").
    static CommandResult moveTo (juce::ValueTree child, int toPosition, const juce::String& what, juce::UndoManager& undoManager);
};

} // namespace model::commands
