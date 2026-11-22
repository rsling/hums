// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Exactly once: the ' in 1'000 separates digits and opens no character
// literal. So the comment after the first number is a comment, and the
// subscript in it is no hit; and the subscript after the second number is
// code, and is one.
const int ticksPerBeat = 1'000; /* the wrong way to read it would be
    grid["stepLength"], which this comment only talks about */

int stepLengthOf (const juce::ValueTree& grid)
{
    return 1'000 * static_cast<int> (grid["stepLength"]);
}
