#pragma once

#include <cstdint>
#include <vector>

namespace io
{

// One event of a Standard MIDI File's track, as the file holds it: where it
// stands, and its bytes. Nothing is made of the bytes here; what a tempo or a
// note is, SmfImport and SmfExport say.
//
// `status` says which of the four kinds of event a file has this is:
//
//   0x80 to 0xEF  a channel message. `bytes` are its data bytes, one for a
//                 program change and a channel pressure (0xC0, 0xD0), two for
//                 every other, each 0 to 127.
//   0xF0          a SysEx event. `bytes` is what follows the length in the
//                 file: the message without its F0, and with the F7 that ends
//                 it where the file has one.
//   0xF7          an escape, or the continuation of a SysEx sent in packets.
//                 `bytes` is what follows the length.
//   0xFF          a meta event of the type `metaType`. `bytes` is its
//                 payload. End of Track is no event: it is the track's
//                 endTick (SmfTrack).
//
// Std only, a plain value.
struct SmfEvent
{
    static constexpr std::uint8_t sysexStatus = 0xf0;
    static constexpr std::uint8_t escapeStatus = 0xf7;
    static constexpr std::uint8_t metaStatus = 0xff;

    static constexpr std::uint8_t endOfTrackMetaType = 0x2f;

    // In the ticks of the file's division, counted from the start of the
    // track, not from the event before.
    std::int64_t tick = 0;

    std::uint8_t status = 0;

    // Of a meta event; 0 for every other.
    std::uint8_t metaType = 0;

    std::vector<std::uint8_t> bytes;

    bool isChannelMessage() const { return status >= 0x80 && status < 0xf0; }
    bool isSysex() const { return status == sysexStatus; }
    bool isEscape() const { return status == escapeStatus; }
    bool isMeta() const { return status == metaStatus; }

    // How many data bytes the channel message with this status byte has.
    static int channelDataSize (std::uint8_t channelStatus)
    {
        const auto kind = channelStatus & 0xf0;
        return (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
    }

    bool operator== (const SmfEvent&) const = default;
};

} // namespace io
