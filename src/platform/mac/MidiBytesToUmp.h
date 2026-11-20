#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace platform
{

// Converts one complete MIDI 1.0 byte message into Universal MIDI Packet words
// (MIDI 1.0 protocol, group 0), the form MIDISendEventList takes. Channel
// voice messages become one type-2 word, system common and real-time one
// type-1 word, SysEx a run of type-3 packets of two words each, carrying up to
// six payload bytes. Pure arithmetic: no allocation, safe on the engine thread.
class MidiBytesToUmp
{
public:
    // Words needed for the largest SysEx the document allows (64 KB payload).
    static constexpr std::size_t maxSysexPayload = 64 * 1024;
    static constexpr std::size_t maxWords = 2 * ((maxSysexPayload + 5) / 6);

    // Returns the number of words written, or 0 if the bytes aren't exactly
    // one well-formed message or the words don't fit.
    static std::size_t convert (std::span<const std::uint8_t> message,
                                std::span<std::uint32_t> words) noexcept;
};

} // namespace platform
