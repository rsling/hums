// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: a static member defined outside its class is a second
// definition of a schema name like any other. The function below it has a
// qualified name too and returns a ValueTree; it spells no name and is no hit.
const juce::Identifier ClipModel::type ("Clip");

juce::ValueTree ClipModel::makeNode (const juce::String& name)
{
    juce::ValueTree node (model::ids::Clip);
    node.setProperty (model::ids::name, name.isEmpty() ? "kind" : name, nullptr);
    return node;
}
