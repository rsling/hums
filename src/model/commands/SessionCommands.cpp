#include "model/commands/SessionCommands.h"

#include "model/Ids.h"
#include "model/SceneModel.h"
#include "model/SessionModel.h"
#include "model/commands/NodeLookup.h"

namespace model::commands
{

namespace
{

    juce::ValueTree sessionOf (const juce::ValueTree& project)
    {
        jassert (project.hasType (ids::Project));
        return project.getChildWithName (ids::Session);
    }

    CommandResult noSession()
    {
        return CommandResult::refused ("The project has no Session.");
    }

} // namespace

CommandResult setLaunchQuantise (juce::ValueTree project, std::int64_t ticks, juce::UndoManager& undoManager)
{
    auto session = sessionOf (project);

    if (! session.isValid())
        return noSession();

    if (ticks < 0)
        return CommandResult::refused ("A launch grid is 0 ticks (immediate) or more, not " + juce::String (ticks) + ".");

    // A session that reads as this grid already is left alone, so that the
    // default grid stays unstored and nothing is left to undo.
    if (SessionModel (session).getLaunchQuantise() == ticks)
        return CommandResult::applied();

    session.setProperty (ids::launchQuantise, static_cast<juce::int64> (ticks), &undoManager);

    return CommandResult::applied();
}

CommandResult setSceneName (juce::ValueTree project, int sceneIndex, const juce::String& name, juce::UndoManager& undoManager)
{
    auto session = sessionOf (project);

    if (! session.isValid())
        return noSession();

    if (sceneIndex < 0)
        return CommandResult::refused ("A scene is row 0 or later, not " + juce::String (sceneIndex) + ".");

    auto scene = NodeLookup::childWithNumber (session, ids::Scene, ids::index, sceneIndex);

    if (scene.isValid())
    {
        // A scene that reads as this name already is left alone, so that an
        // empty name that was never stored stays unstored.
        if (SceneModel (scene).getName() == name)
            return CommandResult::applied();

        scene.setProperty (ids::name, name, &undoManager);
        return CommandResult::applied();
    }

    // A row without a node has no name already.
    if (name.isEmpty())
        return CommandResult::applied();

    juce::ValueTree added (ids::Scene);
    added.setProperty (ids::index, sceneIndex, nullptr);
    added.setProperty (ids::name, name, nullptr);

    session.appendChild (added, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
