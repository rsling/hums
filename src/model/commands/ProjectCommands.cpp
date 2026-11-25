#include "model/commands/ProjectCommands.h"

#include "model/Ids.h"
#include "model/ProjectModel.h"

namespace model::commands
{

CommandResult setProjectName (juce::ValueTree project, const juce::String& name, juce::UndoManager& undoManager)
{
    jassert (project.hasType (ids::Project));

    // A project that reads as this name already is left alone, so that an
    // empty name that was never stored stays unstored and nothing is left
    // to undo.
    if (ProjectModel (project).getName() == name)
        return CommandResult::applied();

    project.setProperty (ids::name, name, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
