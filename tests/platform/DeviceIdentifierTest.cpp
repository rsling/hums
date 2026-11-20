// juce::MidiDeviceInfo::identifier -> the CoreMIDI endpoint it names, as
// CoreMidiScheduler::open resolves it. The identifier shapes are the ones
// JUCE 9.0.6 builds in juce_CoreMidi_mac.mm (getConnectedEndpointInfo,
// getEndpointInfo); the ids are unique ids as CoreMIDI hands them out.

#include <catch2/catch_test_macros.hpp>

#include "platform/mac/DeviceIdentifier.h"

#include <cstdint>
#include <string_view>
#include <vector>

using platform::DeviceIdentifier;

namespace
{
    // Does the identifier name an endpoint with this own id and these
    // connections (empty: cabled to nothing)?
    bool names (std::string_view identifier, std::int32_t endpointId, std::vector<std::int32_t> connectionIds = {})
    {
        const auto parsed = DeviceIdentifier::parse (identifier);
        return parsed.has_value() && parsed->names (endpointId, connectionIds);
    }
} // namespace

TEST_CASE ("An endpoint cabled to nothing is named by its own id", "[platform]")
{
    CHECK (names ("-481367999", -481367999));
    CHECK_FALSE (names ("-481367999", 502178978));

    SECTION ("with the id of its device in front")
    {
        CHECK (names ("1245753385 -1103482977", -1103482977));
        CHECK_FALSE (names ("1245753385 -1103482977", 1245753385)); // that is the device
        CHECK_FALSE (names ("1245753385 -1103482977", 1766924080)); // another port of the device
    }
}

TEST_CASE ("An endpoint cabled to external devices is named by what it is cabled to", "[platform]")
{
    const std::int32_t interfaceOut = 999;

    SECTION ("one external device with one port")
    {
        CHECK (names ("222", interfaceOut, { 222 }));
    }

    SECTION ("an external device with several ports: its device id comes first")
    {
        CHECK (names ("111 222", interfaceOut, { 222 }));
        CHECK_FALSE (names ("111 222", interfaceOut, { 111 }));
    }

    SECTION ("several cables, in connection order")
    {
        CHECK (names ("422, 111 333", interfaceOut, { 422, 333 }));
        CHECK_FALSE (names ("422, 111 333", interfaceOut, { 333, 422 }));
    }

    SECTION ("exactly those cables, not some of them")
    {
        CHECK_FALSE (names ("222", interfaceOut, { 222, 333 }));
        CHECK_FALSE (names ("222, 333", interfaceOut, { 222 }));
    }

    SECTION ("its own id no longer names it")
    {
        CHECK_FALSE (names ("999", interfaceOut, { 222 }));
    }
}

TEST_CASE ("Unique ids span the whole 32-bit range", "[platform]")
{
    CHECK (names ("-2147483648", INT32_MIN));
    CHECK (names ("2147483647", INT32_MAX));
    CHECK_FALSE (DeviceIdentifier::parse ("2147483648").has_value());
    CHECK_FALSE (DeviceIdentifier::parse ("-2147483649").has_value());
}

TEST_CASE ("Text that is not an identifier is refused", "[platform]")
{
    CHECK_FALSE (DeviceIdentifier::parse ("").has_value());
    CHECK_FALSE (DeviceIdentifier::parse (" ").has_value());
    CHECK_FALSE (DeviceIdentifier::parse ("IAC Driver Bus 1").has_value());
    CHECK_FALSE (DeviceIdentifier::parse ("12abc").has_value());   // not a whole number
    CHECK_FALSE (DeviceIdentifier::parse ("1.5").has_value());
    CHECK_FALSE (DeviceIdentifier::parse ("+5").has_value());
    CHECK_FALSE (DeviceIdentifier::parse ("1 2 3").has_value());   // an item has at most two numbers
    CHECK_FALSE (DeviceIdentifier::parse ("1,,2").has_value());    // empty item
    CHECK_FALSE (DeviceIdentifier::parse ("1,").has_value());
    CHECK_FALSE (DeviceIdentifier::parse (",1").has_value());
}
