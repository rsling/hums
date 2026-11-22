// model::DeclaredIdentifiers: the identifiers of model/Ids.h as a list, held
// to Ids.h by reading that file.

#include <catch2/catch_test_macros.hpp>

#include "model/DeclaredIdentifiers.h"
#include "model/Ids.h"

#include <string>
#include <vector>

namespace
{

// The names Ids.h declares, top to bottom. Guardrail 1 fixes the form of a
// declaration there, one per line:
//     inline const juce::Identifier pitch { "pitch" };
std::vector<std::string> namesDeclaredInIdsHeader()
{
    const juce::File header (HUMS_IDS_HEADER);
    REQUIRE (header.existsAsFile());

    const juce::String opening ("inline const juce::Identifier ");
    std::vector<std::string> names;

    for (const auto& line : juce::StringArray::fromLines (header.loadFileAsString()))
        if (line.startsWith (opening))
            names.push_back (line.fromFirstOccurrenceOf (opening, false, false).upToFirstOccurrenceOf (" ", false, false).toStdString());

    return names;
}

std::vector<std::string> namesInTheList()
{
    std::vector<std::string> names;

    for (const auto& identifier : model::DeclaredIdentifiers::inDeclarationOrder())
        names.push_back (identifier.toString().toStdString());

    return names;
}

} // namespace

TEST_CASE ("The list of declared identifiers is Ids.h, name for name and in its order", "[model][DeclaredIdentifiers]")
{
    const auto declared = namesDeclaredInIdsHeader();

    // Ids.h was found and read, and not an empty stand-in.
    REQUIRE (declared.size() > 80);

    CHECK (namesInTheList() == declared);
}

TEST_CASE ("A declared identifier has its position, an invented name has none", "[model][DeclaredIdentifiers]")
{
    CHECK (model::DeclaredIdentifiers::positionOf (model::ids::Project) == 0);
    CHECK (model::DeclaredIdentifiers::positionOf (model::ids::Routing) == 1);
    CHECK (model::DeclaredIdentifiers::isDeclared (model::ids::exportLength));

    CHECK (model::DeclaredIdentifiers::positionOf ("swing") == -1);
    CHECK_FALSE (model::DeclaredIdentifiers::isDeclared ("swing"));
}

TEST_CASE ("Properties stand in the list in the order the canonical writer needs", "[model][DeclaredIdentifiers]")
{
    const auto position = [] (const juce::Identifier& identifier) {
        return model::DeclaredIdentifiers::positionOf (identifier);
    };

    // A Placement's table in docs/SCHEMA.md: id, clipId, start, length, offset.
    CHECK (position (model::ids::id) < position (model::ids::clipId));
    CHECK (position (model::ids::clipId) < position (model::ids::start));
    CHECK (position (model::ids::start) < position (model::ids::length));
    CHECK (position (model::ids::length) < position (model::ids::offset));
}
