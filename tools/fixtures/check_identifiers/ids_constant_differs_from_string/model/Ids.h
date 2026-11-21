// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Run with this file as --ids-file: the constant is not named like its string.
#pragma once

namespace model::ids
{

inline const juce::Identifier pitch { "pitch" };
inline const juce::Identifier velocity { "pitch" };

} // namespace model::ids
