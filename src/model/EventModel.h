#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// One non-note MIDI event of an event clip (docs/SCHEMA.md -> Event clip:
// Events). It has no channel; the Track supplies it.
class EventModel
{
public:
    enum class Kind
    {
        cc,              // a = controller, b = value
        pitchBend,       // a = value 0-16383, 8192 = centre
        programChange,   // a = program
        channelPressure, // a = pressure
        polyPressure,    // a = pitch, b = pressure
        sysex            // payload in data
    };

    // The node must be an Event node.
    explicit EventModel (juce::ValueTree eventNode);

    std::int64_t getTick() const;
    Kind getKind() const;
    // What a and b mean depends on the kind; see Kind.
    int getA() const;
    int getB() const;
    // The SysEx payload as hex digits, without F0 and F7; empty for every other kind.
    juce::String getData() const;
    // A disabled event renders nothing but stays editable.
    bool isEnabled() const;

private:
    juce::ValueTree node;
};

} // namespace model
