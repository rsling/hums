#include "platform/mac/MidiBytesToUmp.h"

namespace platform
{

namespace
{
    bool isDataByte (std::uint8_t b) { return b < 0x80; }

    // Total length of a non-SysEx message with this status byte, or 0 if the
    // status can't start a message on its own.
    std::size_t expectedLength (std::uint8_t status)
    {
        if (status < 0x80)
            return 0;

        if (status < 0xf0)
        {
            const auto kind = status & 0xf0;
            return (kind == 0xc0 || kind == 0xd0) ? 2 : 3;
        }

        switch (status)
        {
            case 0xf1: return 2; // MTC quarter frame
            case 0xf2: return 3; // song position pointer
            case 0xf3: return 2; // song select
            case 0xf6: return 1; // tune request
            case 0xf0: case 0xf7: return 0; // SysEx is handled separately
            default: return status >= 0xf8 ? 1 : 0; // real-time; f4, f5 undefined
        }
    }

    std::size_t convertSysex (std::span<const std::uint8_t> message, std::span<std::uint32_t> words)
    {
        if (message.size() < 2 || message.back() != 0xf7)
            return 0;

        const auto payload = message.subspan (1, message.size() - 2);

        if (payload.size() > MidiBytesToUmp::maxSysexPayload)
            return 0;

        for (auto b : payload)
            if (! isDataByte (b))
                return 0;

        const auto packetCount = payload.empty() ? std::size_t { 1 } : (payload.size() + 5) / 6;

        if (words.size() < 2 * packetCount)
            return 0;

        for (std::size_t packet = 0; packet < packetCount; ++packet)
        {
            const auto first = packet * 6;
            const auto count = payload.size() - first < 6 ? payload.size() - first : std::size_t { 6 };

            // 0 = complete in one packet, 1 = start, 2 = continue, 3 = end.
            std::uint32_t status = 0;
            if (packetCount > 1)
                status = packet == 0 ? 1u : (packet == packetCount - 1 ? 3u : 2u);

            std::uint8_t bytes[6] {};
            for (std::size_t i = 0; i < count; ++i)
                bytes[i] = payload[first + i];

            words[2 * packet] = 0x30000000u | (status << 20) | (static_cast<std::uint32_t> (count) << 16)
                              | (std::uint32_t { bytes[0] } << 8) | bytes[1];
            words[2 * packet + 1] = (std::uint32_t { bytes[2] } << 24) | (std::uint32_t { bytes[3] } << 16)
                                  | (std::uint32_t { bytes[4] } << 8) | bytes[5];
        }

        return 2 * packetCount;
    }
} // namespace

std::size_t MidiBytesToUmp::convert (std::span<const std::uint8_t> message,
                                     std::span<std::uint32_t> words) noexcept
{
    if (message.empty() || words.empty())
        return 0;

    const auto status = message[0];

    if (status == 0xf0)
        return convertSysex (message, words);

    const auto length = expectedLength (status);

    if (length == 0 || message.size() != length)
        return 0;

    for (std::size_t i = 1; i < length; ++i)
        if (! isDataByte (message[i]))
            return 0;

    const std::uint32_t data1 = length > 1 ? message[1] : 0;
    const std::uint32_t data2 = length > 2 ? message[2] : 0;
    const std::uint32_t type = status < 0xf0 ? 0x20000000u : 0x10000000u;

    words[0] = type | (std::uint32_t { status } << 16) | (data1 << 8) | data2;
    return 1;
}

} // namespace platform
