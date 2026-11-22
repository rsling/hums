#include "model/commands/PadMapCommands.h"

#include "model/Ids.h"
#include "model/commands/NodeLookup.h"

namespace model::commands
{

namespace
{

    juce::ValueTree padMapOf (const juce::ValueTree& project)
    {
        jassert (project.hasType (ids::Project));
        return project.getChildWithName (ids::Routing).getChildWithName (ids::PadMap);
    }

    juce::ValueTree padOf (const juce::ValueTree& project, int index)
    {
        return NodeLookup::childWithNumber (padMapOf (project), ids::Pad, ids::index, index);
    }

    bool isPitch (int pitch)
    {
        return pitch >= 0 && pitch <= 127;
    }

    CommandResult noSuchPad (int index)
    {
        return CommandResult::refused ("The pad map has no pad " + juce::String (index) + ".");
    }

    CommandResult notAPitch (int pitch)
    {
        return CommandResult::refused ("A pitch is a note from 0 to 127, not " + juce::String (pitch) + ".");
    }

    CommandResult inputPitchTaken (int inputPitch, const juce::ValueTree& holder)
    {
        return CommandResult::refused ("Note " + juce::String (inputPitch) + " is already what pad " + holder[ids::index].toString() + " sends.");
    }

} // namespace

CommandResult addPad (juce::ValueTree project, int index, int inputPitch, int pitch, const juce::String& label, juce::UndoManager& undoManager)
{
    auto padMap = padMapOf (project);

    if (! padMap.isValid())
        return CommandResult::refused ("The project has no pad map.");

    if (index < 0)
        return CommandResult::refused ("A pad number is 0 or more, not " + juce::String (index) + ".");

    if (padOf (project, index).isValid())
        return CommandResult::refused ("The pad map already has a pad " + juce::String (index) + ".");

    if (! isPitch (inputPitch))
        return notAPitch (inputPitch);

    if (! isPitch (pitch))
        return notAPitch (pitch);

    const auto holder = NodeLookup::childWithNumber (padMap, ids::Pad, ids::inputPitch, inputPitch);

    if (holder.isValid())
        return inputPitchTaken (inputPitch, holder);

    juce::ValueTree pad (ids::Pad);
    pad.setProperty (ids::index, index, nullptr);
    pad.setProperty (ids::inputPitch, inputPitch, nullptr);
    pad.setProperty (ids::pitch, pitch, nullptr);
    pad.setProperty (ids::label, label, nullptr);

    padMap.appendChild (pad, &undoManager);

    return CommandResult::applied();
}

CommandResult removePad (juce::ValueTree project, int index, juce::UndoManager& undoManager)
{
    const auto pad = padOf (project, index);

    if (! pad.isValid())
        return noSuchPad (index);

    pad.getParent().removeChild (pad, &undoManager);

    return CommandResult::applied();
}

CommandResult setPadInputPitch (juce::ValueTree project, int index, int inputPitch, juce::UndoManager& undoManager)
{
    auto pad = padOf (project, index);

    if (! pad.isValid())
        return noSuchPad (index);

    if (! isPitch (inputPitch))
        return notAPitch (inputPitch);

    const auto holder = NodeLookup::childWithNumber (pad.getParent(), ids::Pad, ids::inputPitch, inputPitch);

    if (holder.isValid() && holder != pad)
        return inputPitchTaken (inputPitch, holder);

    pad.setProperty (ids::inputPitch, inputPitch, &undoManager);

    return CommandResult::applied();
}

CommandResult setPadPitch (juce::ValueTree project, int index, int pitch, juce::UndoManager& undoManager)
{
    auto pad = padOf (project, index);

    if (! pad.isValid())
        return noSuchPad (index);

    if (! isPitch (pitch))
        return notAPitch (pitch);

    pad.setProperty (ids::pitch, pitch, &undoManager);

    return CommandResult::applied();
}

CommandResult setPadLabel (juce::ValueTree project, int index, const juce::String& label, juce::UndoManager& undoManager)
{
    auto pad = padOf (project, index);

    if (! pad.isValid())
        return noSuchPad (index);

    pad.setProperty (ids::label, label, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
