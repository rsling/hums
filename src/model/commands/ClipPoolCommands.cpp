#include "model/commands/ClipPoolCommands.h"

#include "model/ColourText.h"
#include "model/Ids.h"
#include "model/PropertyReader.h"
#include "model/TracksModel.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/RefcountGate.h"

namespace model::commands
{

namespace
{

    juce::ValueTree clipPoolOf (const juce::ValueTree& project)
    {
        jassert (project.hasType (ids::Project));
        return project.getChildWithName (ids::ClipPool);
    }

    juce::ValueTree clipOf (const juce::ValueTree& project, const juce::String& clipId)
    {
        return NodeLookup::childWithId (clipPoolOf (project), ids::Clip, clipId);
    }

    CommandResult noSuchClip (const juce::String& clipId)
    {
        return CommandResult::refused ("There is no Clip with the id \"" + clipId + "\".");
    }

    // The number of Placements and Slots that reference the clip. Computed
    // from the tracks every time, never stored.
    int refcountOf (const juce::ValueTree& project, const juce::String& clipId)
    {
        return TracksModel (project.getChildWithName (ids::Tracks)).getRefcount (clipId);
    }

    // What both kinds of clip have. `kind` is the value the schema lists,
    // "event" or "step"; a clip keeps it for good.
    juce::ValueTree newClip (const juce::String& id, const juce::String& name, const juce::String& kind)
    {
        juce::ValueTree clip (ids::Clip);
        clip.setProperty (ids::id, id, nullptr);
        clip.setProperty (ids::name, name, nullptr);
        clip.setProperty (ids::kind, kind, nullptr);

        return clip;
    }

} // namespace

CommandResult addEventClip (juce::ValueTree project, const juce::String& name, std::int64_t loopLength, juce::UndoManager& undoManager)
{
    auto clipPool = clipPoolOf (project);

    if (! clipPool.isValid())
        return CommandResult::refused ("The project has no ClipPool.");

    if (loopLength < 1)
        return CommandResult::refused ("The loop of an event clip is at least one tick long, not " + juce::String (loopLength) + ".");

    const auto id = juce::Uuid().toString();

    // Built apart and added whole, so that undo knows one action.
    auto clip = newClip (id, name, "event");
    clip.setProperty (ids::loopLength, static_cast<juce::int64> (loopLength), nullptr);
    clip.appendChild (juce::ValueTree (ids::Events), nullptr);

    clipPool.appendChild (clip, &undoManager);

    return CommandResult::appliedAndCreated (id);
}

CommandResult addStepClip (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager)
{
    auto clipPool = clipPoolOf (project);

    if (! clipPool.isValid())
        return CommandResult::refused ("The project has no ClipPool.");

    const auto id = juce::Uuid().toString();

    // A Grid that stores nothing is the schema's default grid. A step clip
    // stores no loop length: it follows from the Grid.
    auto clip = newClip (id, name, "step");
    clip.appendChild (juce::ValueTree (ids::Grid), nullptr);

    clipPool.appendChild (clip, &undoManager);

    return CommandResult::appliedAndCreated (id);
}

namespace
{

    // Sets one text property of a clip through the refcount gate. A clip
    // that has the value already is left alone, so that an edit that
    // changes nothing makes no clone either.
    CommandResult setClipText (juce::ValueTree project, const juce::String& clipId, const juce::Identifier& property, const juce::String& value, EditContext& editContext, juce::UndoManager& undoManager)
    {
        const auto admission = RefcountGate::admit (project, clipId, editContext, undoManager);

        if (! admission.wasAdmitted())
            return CommandResult::refused (admission.refusal);

        if (PropertyReader::getString (admission.clip, property, "") == value)
            return CommandResult::applied();

        auto clip = RefcountGate::open (project, admission, editContext, undoManager);
        clip.setProperty (property, value, &undoManager);

        return CommandResult::applied();
    }

} // namespace

CommandResult setClipName (juce::ValueTree project, const juce::String& clipId, const juce::String& name, EditContext& editContext, juce::UndoManager& undoManager)
{
    return setClipText (project, clipId, ids::name, name, editContext, undoManager);
}

CommandResult setClipColour (juce::ValueTree project, const juce::String& clipId, const juce::String& colour, EditContext& editContext, juce::UndoManager& undoManager)
{
    if (! ColourText::isValid (colour))
        return CommandResult::refused ("\"" + colour + "\" is not a colour: '#' and eight hexadecimal digits, or nothing for no colour.");

    return setClipText (project, clipId, ids::colour, colour, editContext, undoManager);
}

CommandResult removeClip (juce::ValueTree project, const juce::String& clipId, juce::UndoManager& undoManager)
{
    const auto clip = clipOf (project, clipId);

    if (! clip.isValid())
        return noSuchClip (clipId);

    const auto refcount = refcountOf (project, clipId);

    if (refcount > 0)
        return CommandResult::refused ("The clip is still used " + juce::String (refcount) + " times, by placements or slots; a clip in use cannot be removed.");

    clip.getParent().removeChild (clip, &undoManager);

    return CommandResult::applied();
}

CommandResult removeUnusedClips (juce::ValueTree project, juce::UndoManager& undoManager)
{
    auto clipPool = clipPoolOf (project);

    if (! clipPool.isValid())
        return CommandResult::refused ("The project has no ClipPool.");

    // The list is made first: removing a child while walking the pool would
    // move the ones behind it.
    for (const auto& clip : NodeLookup::childrenOfType (clipPool, ids::Clip))
        if (refcountOf (project, PropertyReader::getRequiredString (clip, ids::id)) == 0)
            clipPool.removeChild (clip, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
