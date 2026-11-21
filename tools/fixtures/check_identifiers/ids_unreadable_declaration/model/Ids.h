// Fixture for tools/check_identifiers.py: this tree must fail the check.
// Run with this file as --ids-file: the second declaration is not of the one
// form the check reads, so its string would go unnoticed.
#pragma once

namespace model::ids
{

inline const juce::Identifier pitch { "pitch" };
static const juce::Identifier velocity ("velocity");

} // namespace model::ids
