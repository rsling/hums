// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A schema name for a member that the list in braces names itself. The
// position in the list says nothing then; the member's name does.
struct RowKey
{
    juce::String label;
    juce::Identifier rowProperty;
};

const RowKey sceneKey = { .label = "index", .rowProperty = "scene" };
