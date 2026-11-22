#pragma once

#include <juce_core/juce_core.h>

namespace model::commands
{

// What a command reports back. A command checks everything before it touches
// the document, so it has either applied completely or refused and changed
// nothing, the undo history included (CLAUDE.md -> Conventions).
//
// "Applied" does not mean "changed": setting a name to the name it already
// has is applied and performs nothing.
struct CommandResult
{
    static CommandResult applied()
    {
        return {};
    }

    // For a command that makes a node with an id of its own.
    static CommandResult appliedAndCreated (const juce::String& idOfNewNode)
    {
        CommandResult result;
        result.createdId = idOfNewNode;
        return result;
    }

    // `reason` says in plain words why not; it must not be empty.
    static CommandResult refused (const juce::String& reason)
    {
        jassert (reason.isNotEmpty());

        CommandResult result;
        result.refusal = reason;
        return result;
    }

    bool wasApplied() const
    {
        return refusal.isEmpty();
    }

    // Why the command was refused; empty when it was applied.
    juce::String refusal;

    // The id of the node the command made; empty for a command that makes
    // none, and when it was refused.
    juce::String createdId;
};

} // namespace model::commands
