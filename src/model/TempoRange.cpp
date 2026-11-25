#include "model/TempoRange.h"

#include "model/CanonicalDouble.h"

#include <cmath>

namespace model
{

bool TempoRange::contains (double bpm)
{
    if (! std::isfinite (bpm))
        return false;

    // The ends as the document would hold them: read through their saved
    // text, as every tempo in the document is, so that a tempo at an end
    // compares as equal to it.
    return bpm >= CanonicalDouble::rounded (lowestBpm) && bpm <= CanonicalDouble::rounded (highestBpm);
}

juce::String TempoRange::inWords()
{
    return CanonicalDouble::textOf (lowestBpm) + " to " + CanonicalDouble::textOf (highestBpm) + " bpm";
}

} // namespace model
