#pragma once

#include "model/commands/EditContext.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// The refcount gate (docs/EDITING.md): every command that changes a clip
// goes through it, and it decides which clip the change lands on.
//
//   - A clip that at most one Placement or Slot references is simply
//     changed.
//   - A clip with more references is changed for all of them ("update
//     all"), unless the gesture's policy is "make unique" and its context
//     names references. Then the first command of the gesture that changes
//     the clip gets a clone: a copy of the clip with a new id for the clip
//     and for every node in it that has one, put into the pool behind the
//     original. Exactly the context's references are redirected to it, the
//     context remembers it, and every later command of the gesture changes
//     the clone and makes no second one. The original stays in the pool,
//     also when nothing references it any more.
//
// A command passes the gate in two steps, because a command checks
// everything before it touches the tree:
//
//     const auto admission = RefcountGate::admit (project, clipId, editContext, undoManager);
//     if (! admission.wasAdmitted())
//         return CommandResult::refused (admission.refusal);
//     ... the command's own checks, read from admission.clip ...
//     auto clip = RefcountGate::open (project, admission, editContext, undoManager);
//     ... the change, to `clip` ...
//
// `admit` changes nothing. `open` makes the clone when one is due, with the
// command's UndoManager, so that it belongs to the gesture's transaction and
// one undo takes back the clone, the redirection and the change together.
class RefcountGate
{
public:
    struct Admission
    {
        bool wasAdmitted() const
        {
            return refusal.isEmpty();
        }

        // Why the command cannot change the clip; empty when it can.
        juce::String refusal;

        // The clip as it stands, for the command's own checks: the one
        // named, or the gesture's clone when it has one. What `open` hands
        // out has the same content.
        juce::ValueTree clip;

        // Whether `open` will make a clone.
        bool needsClone = false;
    };

    // `clipId` names the clip the gesture edits; once the gesture has
    // cloned, the id of the original and that of the clone both stand for
    // the clone. Refused when
    //   - no Clip has this id, or the gesture's clone is gone;
    //   - a reference of the context is not there (no such Placement, no
    //     such Track, an empty slot) or references another clip;
    //   - the context has cloned and the command names a third clip;
    //   - the context belongs to an earlier gesture (this one asserts too).
    static Admission admit (const juce::ValueTree& project, const juce::String& clipId, const EditContext& editContext, const juce::UndoManager& undoManager);

    // The Clip node to change. `admission` is what `admit` has just said
    // for the same project and context, and it was admitted; nothing has
    // changed the document in between.
    static juce::ValueTree open (juce::ValueTree project, const Admission& admission, EditContext& editContext, juce::UndoManager& undoManager);
};

} // namespace model::commands
