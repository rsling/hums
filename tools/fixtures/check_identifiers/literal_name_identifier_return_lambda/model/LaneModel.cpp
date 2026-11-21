// Fixture for tools/check_identifiers.py: this tree must fail the check.
// The same for a lambda that names its return type; "groove" is not a schema
// name. The two declarations in one statement are deliberate: without that
// comma the check also takes the body for the brackets of an Identifier
// construction, and this fixture would not show that the return is read.
static const auto propertyOfRow = [] (int row) -> juce::Identifier {
    const int firstRow = 0, lastRow = 3;
    return row < firstRow || row > lastRow ? "groove" : model::ids::fixedPitch;
};
