// model::CanonicalDouble: a double as the document holds it, six significant
// digits [D14]. docs/SCHEMA.md -> Doubles.

#include <catch2/catch_test_macros.hpp>

#include "model/CanonicalDouble.h"
#include "model/Ids.h"
#include "model/PropertyReader.h"

TEST_CASE ("A double is written with six significant digits", "[model][CanonicalDouble]")
{
    CHECK (model::CanonicalDouble::textOf (118.0) == "118");
    CHECK (model::CanonicalDouble::textOf (118.1234567) == "118.123");
    CHECK (model::CanonicalDouble::textOf (999.9996) == "1000");
    CHECK (model::CanonicalDouble::textOf (1.0 / 3.0) == "0.333333");
    CHECK (model::CanonicalDouble::textOf (0.00001) == "1e-05");
}

TEST_CASE ("The rounded value is the one its text reads back as, and rounding it again changes nothing", "[model][CanonicalDouble]")
{
    for (const auto value : { 118.0, 118.1234567, 0.000099, 0.000765, 1.0 / 3.0, 4.0, 1234567.891, 1e-05 })
    {
        const auto rounded = model::CanonicalDouble::rounded (value);

        CHECK (juce::exactlyEqual (rounded, model::CanonicalDouble::textOf (value).getDoubleValue()));
        CHECK (juce::exactlyEqual (model::CanonicalDouble::rounded (rounded), rounded));
        CHECK (model::CanonicalDouble::textOf (rounded) == model::CanonicalDouble::textOf (value));
    }
}

TEST_CASE ("A double stored as its text is read as the rounded value, where one stored as a number is not", "[model][CanonicalDouble]")
{
    // 0.000099 is one of the values whose fifteen-digit text, which a
    // juce::var holding the number gives, reads back as another number.
    const auto rounded = model::CanonicalDouble::rounded (0.000099);

    juce::ValueTree cell (model::ids::Cell);
    cell.setProperty (model::ids::gate, model::CanonicalDouble::textOf (0.000099), nullptr);

    CHECK (juce::exactlyEqual (model::PropertyReader::getRequiredDouble (cell, model::ids::gate), rounded));
}
