#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace model::commands
{

// What the refcount gate leaves in a gesture's undo transaction when it
// makes a clone: an action that does nothing and says which clone was made
// here. By it an edit context that has cloned knows its own transaction
// (EditContext::belongsToAnEarlierGesture), whatever a later gesture has
// done: the mark of its clone is in the transaction of its gesture and in no
// other.
//
// A juce::UndoManager gives its transactions no name to know them by, and
// the number of a transaction's actions tells nothing: a later gesture can
// have performed as many.
class CloneMark : public juce::UndoableAction
{
public:
    // Puts the mark of that clone into the current transaction of
    // `undoManager`. Once per clone, by the refcount gate, with the
    // UndoManager it makes the clone with.
    static void leaveIn (juce::UndoManager& undoManager, const juce::String& cloneClipId);

    // True while the transaction that holds the mark of that clone is the
    // current one. False once a new one was begun, also by undo or redo
    // (which begin one), and when no such mark was ever left.
    static bool isInCurrentTransaction (const juce::UndoManager& undoManager, const juce::String& cloneClipId);

    // Nothing to do and nothing to take back.
    bool perform() override;
    bool undo() override;

private:
    explicit CloneMark (const juce::String& cloneClipId);

    juce::String cloneId;

    JUCE_LEAK_DETECTOR (CloneMark)
};

} // namespace model::commands
