#include "DocumentChecks.h"

#include "io/ProjectFile.h"
#include "model/DeclaredIdentifiers.h"
#include "model/Ids.h"
#include "model/Validator.h"

#include <algorithm>

namespace guardrails
{

namespace
{

    namespace ids = model::ids;

    // Walks one node and everything below it. `path` is the way to the node
    // in node types; it need not tell siblings apart, the name it is printed
    // with does that well enough.
    void collectUndeclaredNames (const juce::ValueTree& node, const juce::String& path, std::vector<std::string>& failures)
    {
        if (! model::DeclaredIdentifiers::isDeclared (node.getType()))
            failures.push_back (("undeclared node type \"" + node.getType().toString() + "\" at " + path).toStdString());

        for (int i = 0; i < node.getNumProperties(); ++i)
        {
            const auto property = node.getPropertyName (i);

            if (! model::DeclaredIdentifiers::isDeclared (property))
                failures.push_back (("undeclared property \"" + property.toString() + "\" at " + path).toStdString());
        }

        for (const auto& child : node)
        {
            // UiState is not part of the undoable subtree, and its children
            // belong to the views.
            if (node.hasType (ids::Project) && child.hasType (ids::UiState))
                continue;

            collectUndeclaredNames (child, path + "/" + child.getType().toString(), failures);
        }
    }

    // Where two texts part, for a finding somebody has to read.
    std::string firstDifferenceBetween (const juce::String& first, const juce::String& second)
    {
        const auto firstLines = juce::StringArray::fromLines (first);
        const auto secondLines = juce::StringArray::fromLines (second);
        const auto commonLines = std::min (firstLines.size(), secondLines.size());

        for (int line = 0; line < commonLines; ++line)
            if (firstLines[line] != secondLines[line])
                return ("line " + juce::String (line + 1) + ": first save \"" + firstLines[line] + "\", second save \"" + secondLines[line] + "\"").toStdString();

        return ("the first save has " + juce::String (firstLines.size()) + " lines, the second " + juce::String (secondLines.size())).toStdString();
    }

} // namespace

std::vector<std::string> DocumentChecks::failuresOf (const juce::ValueTree& project)
{
    auto failures = invariantFailuresOf (project);

    for (const auto& failure : undeclaredNamesIn (project))
        failures.push_back (failure);

    // An invalid document is refused by the load path, which would only say
    // the same again.
    if (failures.empty())
        failures = saveLoadSaveFailuresOf (project);

    return failures;
}

std::vector<std::string> DocumentChecks::invariantFailuresOf (const juce::ValueTree& project)
{
    std::vector<std::string> failures;

    for (const auto& violation : model::Validator::validate (project))
    {
        auto where = violation.nodePath;

        if (violation.property.isNotEmpty())
            where << ", " << violation.property;

        failures.push_back (("invalid: " + where + ": " + violation.reason).toStdString());
    }

    return failures;
}

std::vector<std::string> DocumentChecks::undeclaredNamesIn (const juce::ValueTree& project)
{
    std::vector<std::string> failures;
    collectUndeclaredNames (project, project.getType().toString(), failures);

    return failures;
}

std::vector<std::string> DocumentChecks::saveLoadSaveFailuresOf (const juce::ValueTree& project)
{
    const auto firstSave = io::ProjectFile::toCanonicalXml (project);

    juce::ValueTree loaded;
    juce::UndoManager undoManager;
    const auto result = io::ProjectFile::loadFromText (firstSave, loaded, undoManager);

    if (! result.wasLoaded())
        return { ("the saved document was not loaded again: " + result.message).toStdString() };

    const auto secondSave = io::ProjectFile::toCanonicalXml (loaded);

    if (secondSave != firstSave)
        return { "save -> load -> save is not byte-identical; " + firstDifferenceBetween (firstSave, secondSave) };

    return {};
}

} // namespace guardrails
