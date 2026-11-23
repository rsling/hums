#include "model/PropertyReader.h"

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <system_error>

namespace model
{

namespace
{

    bool isDigit (char character)
    {
        return character >= '0' && character <= '9';
    }

    // Advances position over a run of digits; false if there is none.
    bool skipDigits (const std::string& text, std::size_t& position)
    {
        const auto first = position;

        while (position < text.size() && isDigit (text[position]))
            ++position;

        return position > first;
    }

    // Digits with an optional leading '-'. std::from_chars alone would do the
    // same, but it reports "did not use all of the text" only through its
    // pointer, so the whole-text check is spelled out here.
    template <typename Integer>
    std::optional<Integer> parseInteger (const std::string& text)
    {
        Integer value = 0;
        const auto* begin = text.data();
        const auto* end = begin + text.size();
        const auto [stoppedAt, error] = std::from_chars (begin, end, value, 10);

        // from_chars accepts nothing but '-' and digits in base 10; a value
        // that does not fit the type is an error too.
        if (error != std::errc() || stoppedAt != end || begin == end)
            return std::nullopt;

        return value;
    }

    // No double has an exponent of more than three digits ("1e+308" is about
    // the largest, "1e-323" about the smallest), and the canonical writer
    // emits no more.
    constexpr std::size_t maxExponentDigits = 3;

    // [-] digits [. digits] [e|E [+|-] digits]: what juce::String (double)
    // and juce::var::toString() write for a finite number, and nothing looser
    // ("inf", "nan", "+1", " 1", ".5", "1." are all refused).
    //
    // The exponent has at most three digits. A longer one is not merely
    // unusual: JUCE's parser adds an exponent's digits up in an int, so
    // "1e4294967297" would overflow it and read as 10 instead of being
    // caught as not finite below.
    bool isDecimalNumber (const std::string& text)
    {
        std::size_t position = 0;

        if (position < text.size() && text[position] == '-')
            ++position;

        if (! skipDigits (text, position))
            return false;

        if (position < text.size() && text[position] == '.')
        {
            ++position;

            if (! skipDigits (text, position))
                return false;
        }

        if (position < text.size() && (text[position] == 'e' || text[position] == 'E'))
        {
            ++position;

            if (position < text.size() && (text[position] == '+' || text[position] == '-'))
                ++position;

            const auto firstExponentDigit = position;

            if (! skipDigits (text, position))
                return false;

            if (position - firstExponentDigit > maxExponentDigits)
                return false;
        }

        return position == text.size();
    }

    std::optional<double> parseDouble (const juce::String& text)
    {
        if (! isDecimalNumber (text.toStdString()))
            return std::nullopt;

        // JUCE's parser rather than std::strtod, which follows the C locale's
        // decimal separator.
        const auto value = text.getDoubleValue();

        // A well-formed number can still be too large for a double ("1e999").
        if (! std::isfinite (value))
            return std::nullopt;

        return value;
    }

    template <typename Value>
    PropertyReader::Result<Value> resultFrom (const std::optional<Value>& parsed)
    {
        if (! parsed.has_value())
            return { PropertyReader::State::malformed, {} };

        return { PropertyReader::State::valid, *parsed };
    }

    // The wrappers' side of the contract: absent means the default, malformed
    // must not happen in a validated document.
    template <typename Value>
    Value valueOr (const PropertyReader::Result<Value>& result, const Value& valueWhenAbsent)
    {
        jassert (! result.isMalformed());
        return result.isValid() ? result.value : valueWhenAbsent;
    }

    // A required property: anything but a valid value is a broken promise.
    template <typename Value>
    Value requiredValue (const PropertyReader::Result<Value>& result)
    {
        jassert (result.isValid());
        return result.isValid() ? result.value : Value {};
    }

} // namespace

PropertyReader::Result<int> PropertyReader::readInt (const juce::ValueTree& node, const juce::Identifier& property)
{
    const auto* stored = node.getPropertyPointer (property);

    if (stored == nullptr)
        return {};

    return resultFrom (parseInteger<int> (stored->toString().toStdString()));
}

PropertyReader::Result<std::int64_t> PropertyReader::readInt64 (const juce::ValueTree& node, const juce::Identifier& property)
{
    const auto* stored = node.getPropertyPointer (property);

    if (stored == nullptr)
        return {};

    return resultFrom (parseInteger<std::int64_t> (stored->toString().toStdString()));
}

PropertyReader::Result<bool> PropertyReader::readBool (const juce::ValueTree& node, const juce::Identifier& property)
{
    const auto* stored = node.getPropertyPointer (property);

    if (stored == nullptr)
        return {};

    const auto text = stored->toString();

    if (text == "1")
        return { State::valid, true };

    if (text == "0")
        return { State::valid, false };

    return { State::malformed, false };
}

PropertyReader::Result<double> PropertyReader::readDouble (const juce::ValueTree& node, const juce::Identifier& property)
{
    const auto* stored = node.getPropertyPointer (property);

    if (stored == nullptr)
        return {};

    return resultFrom (parseDouble (stored->toString()));
}

PropertyReader::Result<juce::String> PropertyReader::readString (const juce::ValueTree& node, const juce::Identifier& property)
{
    const auto* stored = node.getPropertyPointer (property);

    if (stored == nullptr)
        return {};

    return { State::valid, stored->toString() };
}

int PropertyReader::getInt (const juce::ValueTree& node, const juce::Identifier& property, int valueWhenAbsent)
{
    return valueOr (readInt (node, property), valueWhenAbsent);
}

std::int64_t PropertyReader::getInt64 (const juce::ValueTree& node, const juce::Identifier& property, std::int64_t valueWhenAbsent)
{
    return valueOr (readInt64 (node, property), valueWhenAbsent);
}

bool PropertyReader::getBool (const juce::ValueTree& node, const juce::Identifier& property, bool valueWhenAbsent)
{
    return valueOr (readBool (node, property), valueWhenAbsent);
}

double PropertyReader::getDouble (const juce::ValueTree& node, const juce::Identifier& property, double valueWhenAbsent)
{
    return valueOr (readDouble (node, property), valueWhenAbsent);
}

juce::String PropertyReader::getString (const juce::ValueTree& node, const juce::Identifier& property, const juce::String& valueWhenAbsent)
{
    return valueOr (readString (node, property), valueWhenAbsent);
}

int PropertyReader::getRequiredInt (const juce::ValueTree& node, const juce::Identifier& property)
{
    return requiredValue (readInt (node, property));
}

std::int64_t PropertyReader::getRequiredInt64 (const juce::ValueTree& node, const juce::Identifier& property)
{
    return requiredValue (readInt64 (node, property));
}

double PropertyReader::getRequiredDouble (const juce::ValueTree& node, const juce::Identifier& property)
{
    return requiredValue (readDouble (node, property));
}

juce::String PropertyReader::getRequiredString (const juce::ValueTree& node, const juce::Identifier& property)
{
    return requiredValue (readString (node, property));
}

} // namespace model
