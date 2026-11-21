// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The same for a lambda that names its return type; "groove" is not a schema
// name.
static const auto propertyOfRow = [] (int row) -> juce::Identifier {
    return row < 0 || row > 3 ? "groove" : model::ids::fixedPitch;
};
