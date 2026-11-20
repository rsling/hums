#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace platform
{

// juce::MidiDeviceInfo::identifier of a macOS MIDI port, parsed, so that
// CoreMidiScheduler can find the CoreMIDI endpoint JUCE meant by it.
//
// JUCE 9.0.6 builds the identifier from decimal kMIDIPropertyUniqueID values
// (juce_CoreMidi_mac.mm, getConnectedEndpointInfo and getEndpointInfo). One
// item is
//   "<endpoint id>"              or
//   "<device id> <endpoint id>"  when the endpoint's name does not start with
//                                its device's name.
// An endpoint that is cabled to nothing in Audio MIDI Setup has one item,
// naming itself. An endpoint cabled to external devices has one item per
// connected external endpoint instead, in connection order, joined by ", ";
// its own id does not appear. An external device with several ports gives the
// two-number form ("Port 1" does not start with the device's name).
//
// Only the last number of an item identifies anything: the number before it
// is the id of the device that owns it.
class DeviceIdentifier
{
public:
    // Returns nullopt if the text is not a list of such items.
    static std::optional<DeviceIdentifier> parse (std::string_view text);

    // True if this is the identifier JUCE gives an endpoint with this unique
    // id and these connections: the ids its kMIDIPropertyConnectionUniqueID
    // lists that still resolve to an object, in that order (empty for an
    // endpoint that is cabled to nothing).
    bool names (std::int32_t endpointId, std::span<const std::int32_t> connectionIds) const;

private:
    std::vector<std::int32_t> itemIds; // the last number of each item, in order
};

} // namespace platform
