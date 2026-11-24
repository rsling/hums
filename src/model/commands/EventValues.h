#pragma once

#include "model/EventModel.h"

#include <juce_core/juce_core.h>

#include <cstdint>

namespace model::commands
{

// What an event that is no note is made of (docs/SCHEMA.md -> Event).
struct EventValues
{
    // 0 or later.
    std::int64_t tick = 0;
    EventModel::Kind kind = EventModel::Kind::cc;
    // What a and b mean depends on the kind (EventModel::Kind), and so do
    // their ranges. One that the kind does not use is 0.
    int a = 0;
    int b = 0;
    // The payload of a sysex event as hexadecimal digits, two to a byte,
    // without F0 and F7, at most 64 KB; empty for every other kind.
    juce::String data;
    bool enabled = true;
};

} // namespace model::commands
