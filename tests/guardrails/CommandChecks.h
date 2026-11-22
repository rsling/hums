#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <string>
#include <vector>

namespace guardrails
{

// What the property tests hold of every command they run on a document
// (docs/GUARDRAILS.md -> 3). The command runs as a gesture of its own, in
// one undo transaction, and after it:
//
//   - the document still passes the document checks (DocumentChecks: the
//     Validator, declared names only [D15], save -> load -> save);
//   - the command did what was expected of it: a valid one applied, an
//     invalid one was refused;
//   - a refused command left the undoable subtree XML-identical and the undo
//     history unchanged;
//   - an applied command is taken back by undo, to an XML-identical undoable
//     subtree, and brought back by redo; one that performed nothing changed
//     nothing either;
//   - every clip that a Placement or a Slot referenced before the command is
//     still there and of the same kind.
//
// The undoable subtree is everything but UiState. The checks give their
// findings back as text and assert nothing themselves, so that a test can
// show each of them finding what it is there to find.
class CommandChecks
{
public:
    // What whoever ran the command says of it.
    struct Attempt
    {
        // The command and its arguments, for a finding somebody has to read.
        std::string description;

        // Whether the command had to apply (a valid form) or to refuse.
        bool expectedToApply = false;

        // What the command reported.
        bool wasApplied = false;
    };

    // Begins a transaction, runs `command`, which applies one command to
    // `project` with `undoManager`, and holds the outcome to the checks
    // above. `project` must have passed the document checks before: they
    // are run again only on a document the command has changed. One line of text per finding, each naming the command; empty
    // when all hold. An applied command is still applied afterwards.
    static std::vector<std::string> failuresOf (juce::ValueTree project, juce::UndoManager& undoManager, const std::function<Attempt()>& command);
};

} // namespace guardrails
