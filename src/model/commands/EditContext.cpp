#include "model/commands/EditContext.h"

#include <algorithm>

namespace model::commands
{

EditContext::EditContext (Settings::SharedEditPolicy policyOfTheGesture)
    : policy (policyOfTheGesture)
{
}

void EditContext::addPlacement (const juce::String& placementId)
{
    jassert (! hasClone());

    if (std::find (placementIds.begin(), placementIds.end(), placementId) == placementIds.end())
        placementIds.push_back (placementId);
}

void EditContext::addSlot (const juce::String& trackId, int scene)
{
    jassert (! hasClone());

    for (const auto& slot : slots)
        if (slot.trackId == trackId && slot.scene == scene)
            return;

    slots.push_back ({ trackId, scene });
}

Settings::SharedEditPolicy EditContext::getPolicy() const
{
    return policy;
}

const std::vector<juce::String>& EditContext::getPlacementIds() const
{
    return placementIds;
}

const std::vector<EditContext::SlotReference>& EditContext::getSlots() const
{
    return slots;
}

bool EditContext::hasReferences() const
{
    return ! placementIds.empty() || ! slots.empty();
}

bool EditContext::hasClone() const
{
    return cloneId.isNotEmpty();
}

juce::String EditContext::getCloneId() const
{
    return cloneId;
}

juce::String EditContext::getOriginalId() const
{
    return originalId;
}

bool EditContext::belongsToAnEarlierGesture (const juce::UndoManager& undoManager) const
{
    // Within one transaction the number of its actions only grows. A new
    // transaction, also the one that undo and redo begin, starts at none.
    return undoManager.getNumActionsInCurrentTransaction() < actionsWhenLastUsed;
}

void EditContext::rememberClone (const juce::String& originalClipId, const juce::String& cloneClipId)
{
    jassert (! hasClone());
    jassert (originalClipId.isNotEmpty() && cloneClipId.isNotEmpty());

    originalId = originalClipId;
    cloneId = cloneClipId;
}

void EditContext::rememberUse (const juce::UndoManager& undoManager)
{
    actionsWhenLastUsed = undoManager.getNumActionsInCurrentTransaction();
}

} // namespace model::commands
