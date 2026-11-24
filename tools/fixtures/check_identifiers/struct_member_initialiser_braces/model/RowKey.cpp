// Fixture for tools/check_identifiers.py: this tree must fail the check.
// As struct_member_initialiser, with braces, and the constructor defined in
// the class.
class RowKey
{
public:
    RowKey() : label { "index" }, rowProperty { "scene" } {}

private:
    juce::String label;
    juce::Identifier rowProperty;
};
