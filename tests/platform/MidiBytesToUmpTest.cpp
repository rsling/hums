// MIDI 1.0 bytes -> UMP words, as CoreMidiScheduler hands them to
// MIDISendEventList. Expected words worked out by hand from the UMP spec
// (M2-104-UM): type 2 channel voice, type 1 system, type 3 SysEx7.

#include <catch2/catch_test_macros.hpp>

#include "platform/mac/MidiBytesToUmp.h"

#include <array>
#include <cstdint>
#include <vector>

using platform::MidiBytesToUmp;

namespace
{
    std::vector<std::uint32_t> convert (std::vector<std::uint8_t> bytes)
    {
        std::vector<std::uint32_t> words (MidiBytesToUmp::maxWords);
        words.resize (MidiBytesToUmp::convert (bytes, words));
        return words;
    }
} // namespace

TEST_CASE ("Channel voice messages become one type-2 word", "[platform]")
{
    CHECK (convert ({ 0x93, 60, 100 }) == std::vector<std::uint32_t> { 0x20933c64 });
    CHECK (convert ({ 0x80, 60, 64 }) == std::vector<std::uint32_t> { 0x20803c40 });
    CHECK (convert ({ 0xbf, 74, 127 }) == std::vector<std::uint32_t> { 0x20bf4a7f });
    CHECK (convert ({ 0xe0, 0x00, 0x40 }) == std::vector<std::uint32_t> { 0x20e00040 });
    CHECK (convert ({ 0xc2, 5 }) == std::vector<std::uint32_t> { 0x20c20500 });
    CHECK (convert ({ 0xd0, 99 }) == std::vector<std::uint32_t> { 0x20d06300 });
}

TEST_CASE ("System common and real-time messages become one type-1 word", "[platform]")
{
    CHECK (convert ({ 0xf8 }) == std::vector<std::uint32_t> { 0x10f80000 });
    CHECK (convert ({ 0xfa }) == std::vector<std::uint32_t> { 0x10fa0000 });
    CHECK (convert ({ 0xf2, 0x10, 0x02 }) == std::vector<std::uint32_t> { 0x10f21002 });
    CHECK (convert ({ 0xf3, 7 }) == std::vector<std::uint32_t> { 0x10f30700 });
}

TEST_CASE ("SysEx becomes type-3 packets of up to six bytes", "[platform]")
{
    SECTION ("empty payload: one complete packet")
    {
        CHECK (convert ({ 0xf0, 0xf7 }) == std::vector<std::uint32_t> { 0x30000000, 0x00000000 });
    }

    SECTION ("six bytes fit one complete packet")
    {
        CHECK (convert ({ 0xf0, 1, 2, 3, 4, 5, 6, 0xf7 })
               == std::vector<std::uint32_t> { 0x30060102, 0x03040506 });
    }

    SECTION ("seven bytes: start packet of six, end packet of one")
    {
        CHECK (convert ({ 0xf0, 1, 2, 3, 4, 5, 6, 7, 0xf7 })
               == std::vector<std::uint32_t> { 0x30160102, 0x03040506, 0x30310700, 0x00000000 });
    }

    SECTION ("fourteen bytes: start, continue, end")
    {
        CHECK (convert ({ 0xf0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0xf7 })
               == std::vector<std::uint32_t> { 0x30160102, 0x03040506,
                                               0x30260708, 0x090a0b0c,
                                               0x30320d0e, 0x00000000 });
    }

    SECTION ("the largest payload the document allows fits maxWords")
    {
        std::vector<std::uint8_t> bytes (MidiBytesToUmp::maxSysexPayload + 2, 0x11);
        bytes.front() = 0xf0;
        bytes.back() = 0xf7;
        CHECK (convert (bytes).size() == MidiBytesToUmp::maxWords);
    }
}

TEST_CASE ("Malformed messages are refused", "[platform]")
{
    CHECK (convert ({}).empty());
    CHECK (convert ({ 60, 100 }).empty());            // no status byte
    CHECK (convert ({ 0x90, 60 }).empty());           // too short
    CHECK (convert ({ 0x90, 60, 100, 1 }).empty());   // too long
    CHECK (convert ({ 0x90, 0x80, 100 }).empty());    // status where data belongs
    CHECK (convert ({ 0xf0, 1, 2 }).empty());         // SysEx without F7
    CHECK (convert ({ 0xf0, 1, 0x90, 0xf7 }).empty()); // status inside SysEx
    CHECK (convert ({ 0xf4 }).empty());               // undefined system common
    CHECK (convert ({ 0xf7 }).empty());               // stray end of SysEx

    std::vector<std::uint8_t> tooLong (MidiBytesToUmp::maxSysexPayload + 3, 0x11);
    tooLong.front() = 0xf0;
    tooLong.back() = 0xf7;
    CHECK (convert (tooLong).empty());
}

TEST_CASE ("Too little room for the words is refused, not truncated", "[platform]")
{
    const std::array<std::uint8_t, 9> sysex { 0xf0, 1, 2, 3, 4, 5, 6, 7, 0xf7 };
    std::array<std::uint32_t, 3> words {};
    CHECK (MidiBytesToUmp::convert (sysex, words) == 0);
}
