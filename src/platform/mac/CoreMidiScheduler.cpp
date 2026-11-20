#include "platform/mac/CoreMidiScheduler.h"

#include "platform/mac/MidiBytesToUmp.h"

#include <algorithm>
#include <optional>
#include <string_view>

namespace platform
{

namespace
{
    std::optional<SInt32> uniqueIdOf (MIDIObjectRef object)
    {
        SInt32 id = 0;
        if (MIDIObjectGetIntegerProperty (object, kMIDIPropertyUniqueID, &id) != noErr)
            return std::nullopt;
        return id;
    }

    // The external endpoints a destination is cabled to in Audio MIDI Setup.
    std::vector<SInt32> connectionIdsOf (MIDIEndpointRef endpoint)
    {
        std::vector<SInt32> result;
        CFDataRef data = nullptr;

        if (MIDIObjectGetDataProperty (endpoint, kMIDIPropertyConnectionUniqueID, &data) != noErr || data == nullptr)
            return result;

        const auto count = static_cast<std::size_t> (CFDataGetLength (data)) / sizeof (SInt32);
        const auto* raw = reinterpret_cast<const SInt32*> (CFDataGetBytePtr (data));

        for (std::size_t i = 0; i < count; ++i)
            result.push_back (static_cast<SInt32> (CFSwapInt32BigToHost (static_cast<std::uint32_t> (raw[i]))));

        CFRelease (data);
        return result;
    }

    // JUCE's MidiDeviceInfo::identifier on macOS is one of
    //   "<endpoint id>",
    //   "<device id> <endpoint id>"            (multi-entity devices),
    //   "<external id>, <external id>, ..."    (endpoint cabled to external
    //                                          devices in Audio MIDI Setup),
    // all decimal kMIDIPropertyUniqueID values (juce_CoreMidi_mac.mm,
    // getConnectedEndpointInfo / getEndpointInfo at the pinned 9.0.6).
    std::vector<SInt32> idsIn (std::string_view identifier)
    {
        std::vector<SInt32> ids;
        std::size_t i = 0;

        while (i < identifier.size())
        {
            while (i < identifier.size() && (identifier[i] == ' ' || identifier[i] == ','))
                ++i;

            const auto begin = i;
            while (i < identifier.size() && identifier[i] != ' ' && identifier[i] != ',')
                ++i;

            if (i > begin)
            {
                try
                {
                    ids.push_back (static_cast<SInt32> (std::stol (std::string (identifier.substr (begin, i - begin)))));
                }
                catch (...)
                {
                    return {};
                }
            }
        }

        return ids;
    }

    MIDIEndpointRef findDestination (const std::string& identifier)
    {
        const auto ids = idsIn (identifier);
        if (ids.empty())
            return 0;

        const auto contains = [] (const std::vector<SInt32>& list, SInt32 id)
        { return std::find (list.begin(), list.end(), id) != list.end(); };

        const auto count = MIDIGetNumberOfDestinations();

        // A destination whose own id appears in the identifier.
        for (ItemCount i = 0; i < count; ++i)
        {
            const auto endpoint = MIDIGetDestination (i);
            if (const auto own = uniqueIdOf (endpoint); own && contains (ids, *own))
                return endpoint;
        }

        // A destination cabled to exactly the external endpoints named.
        for (ItemCount i = 0; i < count; ++i)
        {
            const auto endpoint = MIDIGetDestination (i);
            const auto connections = connectionIdsOf (endpoint);

            if (! connections.empty()
                && std::all_of (ids.begin(), ids.end(), [&] (SInt32 id) { return contains (connections, id); }))
                return endpoint;
        }

        return 0;
    }

    constexpr std::size_t wordsPerPacket = 64; // MIDIEventPacket::words

    std::size_t eventListBytesFor (std::size_t wordCount)
    {
        const auto packets = (wordCount + wordsPerPacket - 1) / wordsPerPacket;
        return sizeof (MIDIEventList) + packets * sizeof (MIDIEventPacket);
    }
} // namespace

CoreMidiScheduler::CoreMidiScheduler()
    : words (MidiBytesToUmp::maxWords),
      eventListStorage (eventListBytesFor (MidiBytesToUmp::maxWords) / sizeof (std::uint64_t) + 1)
{
    if (MIDIClientCreate (CFSTR ("Hums"), nullptr, nullptr, &client) != noErr)
        return;

    if (MIDIOutputPortCreate (client, CFSTR ("Hums output"), &outputPort) != noErr)
        outputPort = 0;
}

CoreMidiScheduler::~CoreMidiScheduler()
{
    for (auto endpoint : endpoints)
        if (endpoint != 0)
            MIDIFlushOutput (endpoint);

    if (outputPort != 0)
        MIDIPortDispose (outputPort);

    if (client != 0)
        MIDIClientDispose (client);
}

MidiScheduler::PortHandle CoreMidiScheduler::open (const std::string& deviceIdentifier)
{
    if (! isReady())
        return invalidPort;

    const auto endpoint = findDestination (deviceIdentifier);
    if (endpoint == 0)
        return invalidPort;

    for (int handle = 0; handle < maxPorts; ++handle)
    {
        if (endpoints[static_cast<std::size_t> (handle)] == 0)
        {
            endpoints[static_cast<std::size_t> (handle)] = endpoint;
            return handle;
        }
    }

    return invalidPort;
}

void CoreMidiScheduler::close (PortHandle port)
{
    if (port < 0 || port >= maxPorts)
        return;

    auto& endpoint = endpoints[static_cast<std::size_t> (port)];

    // Drops packets CoreMIDI holds for the future on this destination.
    if (endpoint != 0)
        MIDIFlushOutput (endpoint);

    endpoint = 0;
}

bool CoreMidiScheduler::send (PortHandle port,
                              std::span<const std::uint8_t> message,
                              std::int64_t timeNanoseconds) noexcept
{
    if (port < 0 || port >= maxPorts)
        return false;

    const auto endpoint = endpoints[static_cast<std::size_t> (port)];
    if (endpoint == 0)
        return false;

    const auto wordCount = MidiBytesToUmp::convert (message, words);
    if (wordCount == 0)
        return false;

    // Timestamp 0 means "now" to CoreMIDI; a past time is sent at once anyway.
    const MIDITimeStamp timeStamp = machTime.nanosecondsToHost (timeNanoseconds);

    auto* list = reinterpret_cast<MIDIEventList*> (eventListStorage.data());
    const auto listBytes = eventListStorage.size() * sizeof (std::uint64_t);
    auto* packet = MIDIEventListInit (list, kMIDIProtocol_1_0);

    // SysEx packets are two words, so 64-word chunks never split one.
    for (std::size_t first = 0; first < wordCount; first += wordsPerPacket)
    {
        const auto count = std::min (wordsPerPacket, wordCount - first);
        packet = MIDIEventListAdd (list, listBytes, packet, timeStamp, count, words.data() + first);
        if (packet == nullptr)
            return false;
    }

    return MIDISendEventList (outputPort, endpoint, list) == noErr;
}

} // namespace platform
