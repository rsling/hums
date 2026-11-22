#pragma once

#include "model/Settings.h"

#include <juce_data_structures/juce_data_structures.h>

#include <vector>

namespace model::commands
{

// What a gesture carries to every command of it that changes a clip
// (docs/EDITING.md, CLAUDE.md -> Core concepts): the references through
// which the user is editing the clip, the policy for a clip that others
// reference too, and, once make-unique has cloned, which clip was cloned and
// the clone's id.
//
// The view makes one at gesture start, right after beginNewTransaction,
// with the policy of the toolbar toggle (Settings::getSharedEditPolicy), and
// names the references before the first command. All of them reference the
// one clip the gesture edits. A context that names no reference is editing
// at pool level, which is "update all" by definition, whatever its policy.
//
// The context dies with its gesture. One gesture is one undo transaction,
// so a context that meets a later transaction is a programming error; the
// refcount gate asserts on it and refuses the command (RefcountGate).
//
// It is not copied: two copies would each make a clone of their own.
class EditContext
{
public:
    // A slot is named by its track and its row (docs/SCHEMA.md -> Slots).
    struct SlotReference
    {
        juce::String trackId;
        int scene = 0;
    };

    explicit EditContext (Settings::SharedEditPolicy policyOfTheGesture);

    EditContext (const EditContext&) = delete;
    EditContext& operator= (const EditContext&) = delete;

    // A Placement the clip is edited through: one of a track's own or one
    // on an AutomationLane. Naming one twice names it once. Not after the
    // gesture has cloned.
    void addPlacement (const juce::String& placementId);

    // A Slot the clip is edited through. Likewise.
    void addSlot (const juce::String& trackId, int scene);

    Settings::SharedEditPolicy getPolicy() const;
    const std::vector<juce::String>& getPlacementIds() const;
    const std::vector<SlotReference>& getSlots() const;

    // False for pool-level editing.
    bool hasReferences() const;

    // True once make-unique has cloned in this gesture. The context's
    // references then reference the clone, and every further command of the
    // gesture changes the clone.
    bool hasClone() const;

    // The id of the clone, and of the clip it was made from; empty while
    // there is no clone.
    juce::String getCloneId() const;
    juce::String getOriginalId() const;

    // True when the transaction this context has worked in is no longer the
    // current one of `undoManager`: a new one was begun, or it was undone.
    // It is told by what the transaction held when the context was last
    // used; that is always something once the context has cloned. A context
    // whose gesture has left nothing to undo by then cannot be told from a
    // fresh one, and holds no clone that could be wrong.
    bool belongsToAnEarlierGesture (const juce::UndoManager& undoManager) const;

    // For the refcount gate, which is the only one to call these two.
    void rememberClone (const juce::String& originalClipId, const juce::String& cloneClipId);
    void rememberUse (const juce::UndoManager& undoManager);

private:
    Settings::SharedEditPolicy policy;
    std::vector<juce::String> placementIds;
    std::vector<SlotReference> slots;
    juce::String originalId;
    juce::String cloneId;

    // What the gesture's transaction held when the gate last let a command
    // of this context through.
    int actionsWhenLastUsed = 0;
};

} // namespace model::commands
