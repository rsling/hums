// model::PropertyReader: typed reading of ValueTree properties, the same
// whether the property was loaded from XML (a string) or written in this
// session (an int, a double, a bool); "absent" told from "malformed".
// docs/SCHEMA.md -> Conventions.
//
// Doubles are compared with juce::exactlyEqual on purpose: the point is that
// both routes give the very same value.

#include <catch2/catch_test_macros.hpp>

#include "model/Ids.h"
#include "model/PropertyReader.h"

#include <cstdint>
#include <limits>

namespace
{

using model::PropertyReader;
namespace ids = model::ids;

// A node whose one property holds this text, as after ValueTree::fromXml.
juce::ValueTree nodeWithText (const juce::Identifier& property, const juce::String& text)
{
    juce::ValueTree node (ids::Note);
    node.setProperty (property, text, nullptr);
    return node;
}

} // namespace

TEST_CASE ("An absent property is absent for every type", "[model][PropertyReader]")
{
    const juce::ValueTree node (ids::Note);

    CHECK (PropertyReader::readInt (node, ids::pitch).isAbsent());
    CHECK (PropertyReader::readInt64 (node, ids::tick).isAbsent());
    CHECK (PropertyReader::readBool (node, ids::enabled).isAbsent());
    CHECK (PropertyReader::readDouble (node, ids::gate).isAbsent());
    CHECK (PropertyReader::readString (node, ids::name).isAbsent());
}

TEST_CASE ("get returns the given default only when the property is absent", "[model][PropertyReader]")
{
    juce::ValueTree node (ids::Note);

    CHECK (PropertyReader::getInt (node, ids::velocity, 100) == 100);
    CHECK (PropertyReader::getInt64 (node, ids::tick, 7) == 7);
    CHECK (PropertyReader::getBool (node, ids::enabled, true) == true);
    CHECK (juce::exactlyEqual (PropertyReader::getDouble (node, ids::gate, 0.5), 0.5));
    CHECK (PropertyReader::getString (node, ids::name, "none") == "none");

    node.setProperty (ids::velocity, 64, nullptr);
    node.setProperty (ids::tick, std::int64_t { 960 }, nullptr);
    node.setProperty (ids::enabled, false, nullptr);
    node.setProperty (ids::gate, 1.25, nullptr);
    node.setProperty (ids::name, "", nullptr);

    CHECK (PropertyReader::getInt (node, ids::velocity, 100) == 64);
    CHECK (PropertyReader::getInt64 (node, ids::tick, 7) == 960);
    CHECK (PropertyReader::getBool (node, ids::enabled, true) == false);
    CHECK (juce::exactlyEqual (PropertyReader::getDouble (node, ids::gate, 0.5), 1.25));

    // An empty string that is present is not the default.
    CHECK (PropertyReader::getString (node, ids::name, "none") == "");
}

TEST_CASE ("A property reads the same from XML and from a value set in this session", "[model][PropertyReader]")
{
    juce::ValueTree written (ids::Cell);
    written.setProperty (ids::pitch, 60, nullptr);
    written.setProperty (ids::tick, std::int64_t { 5'000'000'000 }, nullptr);
    written.setProperty (ids::on, true, nullptr);
    written.setProperty (ids::gate, 0.6, nullptr);
    written.setProperty (ids::name, "Bass 5-step", nullptr);

    const auto reloaded = juce::ValueTree::fromXml (written.toXmlString());
    REQUIRE (reloaded.isValid());

    // After the reload every property is a string.
    REQUIRE (reloaded.getProperty (ids::pitch).isString());

    for (const auto& node : { written, reloaded })
    {
        CHECK (PropertyReader::readInt (node, ids::pitch).value == 60);
        CHECK (PropertyReader::readInt64 (node, ids::tick).value == 5'000'000'000);
        CHECK (PropertyReader::readBool (node, ids::on).value == true);
        CHECK (juce::exactlyEqual (PropertyReader::readDouble (node, ids::gate).value, 0.6));
        CHECK (PropertyReader::readString (node, ids::name).value == "Bass 5-step");
    }

    CHECK (juce::exactlyEqual (PropertyReader::readDouble (written, ids::gate).value,
                               PropertyReader::readDouble (reloaded, ids::gate).value));
}

TEST_CASE ("Integers are decimal digits with an optional minus sign", "[model][PropertyReader]")
{
    for (const auto* text : { "0", "7", "127", "-1", "-0", "007" })
    {
        INFO ("text: '" << text << "'");
        CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, text), ids::pitch).isValid());
        CHECK (PropertyReader::readInt64 (nodeWithText (ids::tick, text), ids::tick).isValid());
    }

    CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, "007"), ids::pitch).value == 7);
    CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, "-12"), ids::pitch).value == -12);

    for (const auto* text : { "", " ", "+1", " 1", "1 ", "1.0", "1e3", "0x10", "12a", "a", "-", "--1", "1,000" })
    {
        INFO ("text: '" << text << "'");
        CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, text), ids::pitch).isMalformed());
        CHECK (PropertyReader::readInt64 (nodeWithText (ids::tick, text), ids::tick).isMalformed());
    }
}

TEST_CASE ("An integer that does not fit its type is malformed", "[model][PropertyReader]")
{
    const auto intMax = juce::String (std::numeric_limits<int>::max());
    const auto intMin = juce::String (std::numeric_limits<int>::min());

    CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, intMax), ids::pitch).value == std::numeric_limits<int>::max());
    CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, intMin), ids::pitch).value == std::numeric_limits<int>::min());
    CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, "2147483648"), ids::pitch).isMalformed());
    CHECK (PropertyReader::readInt (nodeWithText (ids::pitch, "-2147483649"), ids::pitch).isMalformed());

    // The same text is a fine int64.
    CHECK (PropertyReader::readInt64 (nodeWithText (ids::tick, "2147483648"), ids::tick).value == 2147483648LL);

    CHECK (PropertyReader::readInt64 (nodeWithText (ids::tick, "9223372036854775807"), ids::tick).value
           == std::numeric_limits<std::int64_t>::max());
    CHECK (PropertyReader::readInt64 (nodeWithText (ids::tick, "-9223372036854775808"), ids::tick).value
           == std::numeric_limits<std::int64_t>::min());
    CHECK (PropertyReader::readInt64 (nodeWithText (ids::tick, "9223372036854775808"), ids::tick).isMalformed());
    CHECK (PropertyReader::readInt64 (nodeWithText (ids::tick, "-9223372036854775809"), ids::tick).isMalformed());
}

TEST_CASE ("A bool is exactly 0 or 1", "[model][PropertyReader]")
{
    CHECK (PropertyReader::readBool (nodeWithText (ids::on, "1"), ids::on).value == true);
    CHECK (PropertyReader::readBool (nodeWithText (ids::on, "0"), ids::on).value == false);
    CHECK (PropertyReader::readBool (nodeWithText (ids::on, "0"), ids::on).isValid());

    for (const auto* text : { "", "true", "false", "2", "-1", "01", "1.0", " 1", "yes" })
    {
        INFO ("text: '" << text << "'");
        CHECK (PropertyReader::readBool (nodeWithText (ids::on, text), ids::on).isMalformed());
    }
}

TEST_CASE ("A double is a finite decimal number", "[model][PropertyReader]")
{
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::gate, "0.5"), ids::gate).value, 0.5));
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "118"), ids::bpm).value, 118.0));
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "-2.25"), ids::bpm).value, -2.25));
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "1.5e2"), ids::bpm).value, 150.0));
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "1e+06"), ids::bpm).value, 1000000.0));
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "2.5E-1"), ids::bpm).value, 0.25));

    for (const auto* text : { "", " ", "inf", "-inf", "nan", "NaN", "+1", " 1", "1 ", ".5", "1.", "1e", "1e+", "e5", "1.2.3", "0x1p3", "1,5", "abc", "1e999", "-1e999" })
    {
        INFO ("text: '" << text << "'");
        CHECK (PropertyReader::readDouble (nodeWithText (ids::gate, text), ids::gate).isMalformed());
    }
}

TEST_CASE ("A double's exponent has at most three digits", "[model][PropertyReader]")
{
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "1e+308"), ids::bpm).value, 1e308));
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "1e-005"), ids::bpm).value, 0.00001));
    CHECK (juce::exactlyEqual (PropertyReader::readDouble (nodeWithText (ids::bpm, "5e000"), ids::bpm).value, 5.0));

    // The third is 2^32 + 1: JUCE's parser adds the digits up in an int, in
    // which that exponent comes out as 1 and the text as 10.
    for (const auto* text : { "1e0001", "1e-0005", "1e4294967297", "1e+4294967297", "1e-4294967297", "1E99999999999999999999" })
    {
        INFO ("text: '" << text << "'");
        CHECK (PropertyReader::readDouble (nodeWithText (ids::gate, text), ids::gate).isMalformed());
    }
}

TEST_CASE ("A string property is valid whatever it holds", "[model][PropertyReader]")
{
    CHECK (PropertyReader::readString (nodeWithText (ids::name, ""), ids::name).isValid());
    CHECK (PropertyReader::readString (nodeWithText (ids::name, "Kick+Hat"), ids::name).value == "Kick+Hat");

    // A number written in this session reads as its text.
    juce::ValueTree node (ids::Scene);
    node.setProperty (ids::name, 42, nullptr);
    CHECK (PropertyReader::readString (node, ids::name).value == "42");
}

TEST_CASE ("An identifier constant carries its own name", "[model][Ids]")
{
    // Spot checks: a type, a property, and the constant-equals-string rule
    // that tools/check_identifiers.py enforces over the whole file.
    CHECK (ids::Project.toString() == "Project");
    CHECK (ids::schemaVersion.toString() == "schemaVersion");
    CHECK (juce::Identifier::isValidIdentifier (ids::AutomationLane.toString()));
}

TEST_CASE ("getRequired returns a property that is present", "[model][PropertyReader]")
{
    juce::ValueTree node (ids::TempoEvent);
    node.setProperty (ids::tick, std::int64_t { 7680 }, nullptr);
    node.setProperty (ids::bpm, "118.5", nullptr);
    node.setProperty (ids::numerator, 7, nullptr);
    node.setProperty (ids::name, "Break", nullptr);

    CHECK (PropertyReader::getRequiredInt64 (node, ids::tick) == 7680);
    CHECK (juce::exactlyEqual (PropertyReader::getRequiredDouble (node, ids::bpm), 118.5));
    CHECK (PropertyReader::getRequiredInt (node, ids::numerator) == 7);
    CHECK (PropertyReader::getRequiredString (node, ids::name) == "Break");
}
