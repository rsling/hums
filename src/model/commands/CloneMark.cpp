#include "model/commands/CloneMark.h"

namespace model::commands
{

CloneMark::CloneMark (const juce::String& cloneClipId)
    : cloneId (cloneClipId)
{
}

void CloneMark::leaveIn (juce::UndoManager& undoManager, const juce::String& cloneClipId)
{
    jassert (cloneClipId.isNotEmpty());

    // The UndoManager owns the action from here on.
    const bool wasPerformed = undoManager.perform (new CloneMark (cloneClipId));

    // Refused only inside an undo or a redo, where no command runs.
    jassertquiet (wasPerformed);
}

bool CloneMark::isInCurrentTransaction (const juce::UndoManager& undoManager, const juce::String& cloneClipId)
{
    // Empty right after beginNewTransaction, undo and redo: the current
    // transaction is then one that holds nothing yet.
    juce::Array<const juce::UndoableAction*> actions;
    undoManager.getActionsInCurrentTransaction (actions);

    for (const auto* action : actions)
        if (const auto* mark = dynamic_cast<const CloneMark*> (action))
            if (mark->cloneId == cloneClipId)
                return true;

    return false;
}

bool CloneMark::perform()
{
    return true;
}

bool CloneMark::undo()
{
    return true;
}

} // namespace model::commands
