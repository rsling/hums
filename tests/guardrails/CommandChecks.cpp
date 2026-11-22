#include "CommandChecks.h"

#include "DocumentChecks.h"
#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <map>

namespace guardrails
{

namespace
{

    namespace ids = model::ids;

    // The document without UiState, as text: what undo has to restore.
    juce::String undoableXmlOf (const juce::ValueTree& project)
    {
        auto copy = project.createCopy();

        for (int child = copy.getNumChildren() - 1; child >= 0; --child)
            if (copy.getChild (child).hasType (ids::UiState))
                copy.removeChild (child, nullptr);

        return copy.toXmlString();
    }

    // Every clip id that a Placement or a Slot names, anywhere under the
    // node.
    void collectReferencedClipIds (const juce::ValueTree& node, juce::StringArray& clipIds)
    {
        if (node.hasType (ids::Placement) || node.hasType (ids::Slot))
            clipIds.addIfNotAlreadyThere (model::PropertyReader::getString (node, ids::clipId, ""));

        for (const auto& child : node)
            collectReferencedClipIds (child, clipIds);
    }

    // The kind of every referenced clip, by its id.
    std::map<juce::String, juce::String> kindsOfReferencedClips (const juce::ValueTree& project)
    {
        juce::StringArray referenced;
        collectReferencedClipIds (project.getChildWithName (ids::Tracks), referenced);

        std::map<juce::String, juce::String> kinds;

        for (const auto& clip : project.getChildWithName (ids::ClipPool))
        {
            const auto id = model::PropertyReader::getString (clip, ids::id, "");

            if (clip.hasType (ids::Clip) && referenced.contains (id))
                kinds[id] = model::PropertyReader::getString (clip, ids::kind, "");
        }

        return kinds;
    }

    juce::String kindOfClip (const juce::ValueTree& project, const juce::String& clipId)
    {
        for (const auto& clip : project.getChildWithName (ids::ClipPool))
            if (clip.hasType (ids::Clip) && model::PropertyReader::getString (clip, ids::id, "") == clipId)
                return model::PropertyReader::getString (clip, ids::kind, "");

        return {};
    }

} // namespace

std::vector<std::string> CommandChecks::failuresOf (juce::ValueTree project, juce::UndoManager& undoManager, const std::function<Attempt()>& command)
{
    const auto wholeXmlBefore = project.toXmlString();
    const auto xmlBefore = undoableXmlOf (project);
    const auto kindsBefore = kindsOfReferencedClips (project);
    const auto couldUndoBefore = undoManager.canUndo();
    const auto couldRedoBefore = undoManager.canRedo();

    undoManager.beginNewTransaction();
    const auto attempt = command();

    const auto documentChanged = project.toXmlString() != wholeXmlBefore;
    const auto xmlAfter = documentChanged ? undoableXmlOf (project) : xmlBefore;
    const auto actionsPerformed = undoManager.getNumActionsInCurrentTransaction();

    std::vector<std::string> failures;
    const auto fail = [&failures, &attempt] (const std::string& finding) { failures.push_back (attempt.description + ": " + finding); };

    // A document that is the same text as before would only pass the same
    // checks again: they read every value through its text. That is every
    // refused command and every one that had nothing to change, and the
    // document checks are most of what a command costs here.
    if (documentChanged)
        for (const auto& failure : DocumentChecks::failuresOf (project))
            fail ("the document after it: " + failure);

    if (attempt.expectedToApply && ! attempt.wasApplied)
        fail ("a valid command was refused");

    if (! attempt.expectedToApply && attempt.wasApplied)
        fail ("an invalid command was applied");

    if (! attempt.wasApplied)
    {
        if (xmlAfter != xmlBefore)
            fail ("it was refused and changed the document all the same");

        if (actionsPerformed != 0 || undoManager.canUndo() != couldUndoBefore || undoManager.canRedo() != couldRedoBefore)
            fail ("it was refused and changed the undo history all the same");

        return failures;
    }

    for (const auto& [clipId, kindBefore] : kindsBefore)
    {
        const auto kindNow = kindOfClip (project, clipId);

        if (kindNow.isEmpty())
            fail (("the referenced clip " + clipId + " is gone").toStdString());
        else if (kindNow != kindBefore)
            fail (("the referenced clip " + clipId + " was of kind " + kindBefore + " and is now of kind " + kindNow).toStdString());
    }

    if (actionsPerformed == 0)
    {
        // Nothing to undo: an undo here would take back the command before.
        if (xmlAfter != xmlBefore)
            fail ("it changed the document without the undo history knowing");

        return failures;
    }

    if (! undoManager.undo() || undoableXmlOf (project) != xmlBefore)
        fail ("undo did not bring back the document as it was before");

    if (! undoManager.redo() || undoableXmlOf (project) != xmlAfter)
        fail ("redo did not bring back the document as it was after");

    return failures;
}

} // namespace guardrails
