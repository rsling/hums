#include "model/CanonicalDouble.h"

#include <cmath>

namespace model
{

juce::String CanonicalDouble::textOf (double value)
{
    jassert (std::isfinite (value));
    return juce::String (value);
}

double CanonicalDouble::rounded (double value)
{
    return textOf (value).getDoubleValue();
}

} // namespace model
