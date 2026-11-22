#include "model/commands/SiblingOrder.h"

#include "model/commands/NodeLookup.h"

#include <cstddef>

namespace model::commands
{

CommandResult SiblingOrder::moveTo (juce::ValueTree child, int toPosition, const juce::String& what, juce::UndoManager& undoManager)
{
    auto parent = child.getParent();
    const auto siblings = NodeLookup::childrenOfType (parent, child.getType());
    const auto count = static_cast<int> (siblings.size());

    if (toPosition < 0 || toPosition >= count)
        return CommandResult::refused ("There are " + juce::String (count) + " " + what + ", so a position is 0 to " + juce::String (count - 1) + ", not " + juce::String (toPosition) + ".");

    // Taking the place of the sibling that is there now puts the child
    // behind it when it comes from above and before it when it comes from
    // below: at that position either way.
    parent.moveChild (parent.indexOf (child), parent.indexOf (siblings[static_cast<std::size_t> (toPosition)]), &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
