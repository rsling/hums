// Fixture for tools/check_identifiers.py: this tree must fail the check.
// A list of Identifiers that is a static member defined outside its class.
const std::array<juce::Identifier, 2> Validator::requiredOnNote { model::ids::tick, "length" };
