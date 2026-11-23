#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// Typed reading of ValueTree properties (docs/SCHEMA.md -> Conventions:
// "typed access only").
//
// A property loaded from XML is a string; one written by a command in this
// session is an int, a double or a bool. Every reader here therefore takes
// the property's text and parses that, whatever the juce::var holds, so a
// document reads the same before and after a save and reload. Nothing here
// looks at the var's type.
//
// The text forms are strict, and they are the ones the canonical writer
// emits:
//   int, int64   decimal digits with an optional leading '-', nothing else
//   bool         exactly "0" or "1"
//   double       decimal digits with an optional '-', fraction and exponent;
//                the exponent has at most three digits and the value must
//                be finite. The writer keeps six significant digits [D14];
//                the reader takes any number of those
//   string       any text
//
// read...() tells "absent" from "present and malformed"; the Validator needs
// that distinction. get...() and getRequired...() are for the typed wrappers,
// which read documents that have already passed the Validator.
class PropertyReader
{
public:
    enum class State
    {
        absent,   // the node has no such property; the schema default applies
        valid,
        malformed // present, but not in the text form of the requested type
    };

    template <typename Value>
    struct Result
    {
        State state = State::absent;
        Value value {}; // meaningful only when state is valid

        bool isAbsent() const { return state == State::absent; }
        bool isValid() const { return state == State::valid; }
        bool isMalformed() const { return state == State::malformed; }
    };

    static Result<int> readInt (const juce::ValueTree& node, const juce::Identifier& property);
    static Result<std::int64_t> readInt64 (const juce::ValueTree& node, const juce::Identifier& property);
    static Result<bool> readBool (const juce::ValueTree& node, const juce::Identifier& property);
    static Result<double> readDouble (const juce::ValueTree& node, const juce::Identifier& property);

    // A string property is never malformed.
    static Result<juce::String> readString (const juce::ValueTree& node, const juce::Identifier& property);

    // The property's value, or valueWhenAbsent if the node has no such
    // property. A malformed property is a broken promise — the Validator
    // lets no such document in — so it asserts, and returns valueWhenAbsent.
    static int getInt (const juce::ValueTree& node, const juce::Identifier& property, int valueWhenAbsent);
    static std::int64_t getInt64 (const juce::ValueTree& node, const juce::Identifier& property, std::int64_t valueWhenAbsent);
    static bool getBool (const juce::ValueTree& node, const juce::Identifier& property, bool valueWhenAbsent);
    static double getDouble (const juce::ValueTree& node, const juce::Identifier& property, double valueWhenAbsent);
    static juce::String getString (const juce::ValueTree& node, const juce::Identifier& property, const juce::String& valueWhenAbsent);

    // For a property the schema lists without a default. In a validated
    // document it is always there, so a missing or malformed one asserts;
    // a release build then gets 0, 0.0 or an empty string. (No bool: every
    // bool in the schema has a default.)
    static int getRequiredInt (const juce::ValueTree& node, const juce::Identifier& property);
    static std::int64_t getRequiredInt64 (const juce::ValueTree& node, const juce::Identifier& property);
    static double getRequiredDouble (const juce::ValueTree& node, const juce::Identifier& property);
    static juce::String getRequiredString (const juce::ValueTree& node, const juce::Identifier& property);
};

} // namespace model
