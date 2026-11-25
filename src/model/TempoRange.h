#pragma once

#include <juce_core/juce_core.h>

#include <cstdint>

namespace model
{

// The tempos Hums takes (docs/SCHEMA.md -> Timeline, [D23]): 3.57628 to
// 999.999 bpm, both included. The one place that says so, for the Validator
// and for the command that stores a tempo.
//
// The lower end is what a Standard MIDI File can hold. TempoMap plays a tempo
// as round (60 000 000 / bpm) whole microseconds per quarter note [D13], an
// SMF has 24 bits for that number, and 3.57628 is the smallest tempo of six
// significant digits [D14] whose microseconds fit. The upper end is where the
// three decimals of a bpm end.
class TempoRange
{
public:
    static constexpr double lowestBpm = 3.57628;
    static constexpr double highestBpm = 999.999;

    // The most microseconds per quarter note an SMF tempo event holds.
    static constexpr std::int64_t mostMicrosecondsPerQuarter = 16777215;

    // True for a finite tempo from lowestBpm to highestBpm. The question is
    // put to the value the document holds, i.e. to a tempo of six significant
    // digits (model/CanonicalDouble); a command rounds first and asks then.
    static bool contains (double bpm);

    // "3.57628 to 999.999 bpm", for a message.
    static juce::String inWords();
};

} // namespace model
