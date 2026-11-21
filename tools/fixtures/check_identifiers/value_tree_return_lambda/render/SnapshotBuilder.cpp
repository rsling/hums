// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: the same for a lambda that names its return type. "Event",
// the type, is the hit; "cc", the value of kind, is none.
static const auto makeCcEvent = [] () -> juce::ValueTree {
    return { "Event", { { model::ids::kind, "cc" } } };
};
