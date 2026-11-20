#include "platform/mac/CoreMidiScheduler.h"

#include "platform/mac/DeviceIdentifier.h"
#include "platform/mac/MidiBytesToUmp.h"

#include <algorithm>
#include <chrono>
#include <optional>

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

    // The unique ids of the external objects a destination is cabled to in
    // Audio MIDI Setup, in the order CoreMIDI lists them. Ids that no longer
    // resolve to an object are left out, as JUCE leaves them out of the
    // identifier it builds.
    std::vector<SInt32> connectionIdsOf (MIDIEndpointRef endpoint)
    {
        std::vector<SInt32> result;
        CFDataRef data = nullptr;

        if (MIDIObjectGetDataProperty (endpoint, kMIDIPropertyConnectionUniqueID, &data) != noErr || data == nullptr)
            return result;

        const auto count = static_cast<std::size_t> (CFDataGetLength (data)) / sizeof (SInt32);
        const auto* raw = reinterpret_cast<const SInt32*> (CFDataGetBytePtr (data));

        for (std::size_t i = 0; i < count; ++i)
        {
            const auto id = static_cast<SInt32> (CFSwapInt32BigToHost (static_cast<std::uint32_t> (raw[i])));
            MIDIObjectRef object = 0;
            MIDIObjectType type = kMIDIObjectType_Other;

            if (MIDIObjectFindByUniqueID (id, &object, &type) == noErr)
                result.push_back (id);
        }

        CFRelease (data);
        return result;
    }

    // The destination JUCE lists under this identifier, or 0. Where two
    // destinations would get the same identifier JUCE keeps the first, so the
    // first match is the one.
    MIDIEndpointRef findDestination (const std::string& identifier)
    {
        const auto wanted = DeviceIdentifier::parse (identifier);
        if (! wanted)
            return 0;

        const auto count = MIDIGetNumberOfDestinations();

        for (ItemCount i = 0; i < count; ++i)
        {
            const auto endpoint = MIDIGetDestination (i);
            const auto ownId = uniqueIdOf (endpoint);
            const auto connectionIds = connectionIdsOf (endpoint);

            if (! ownId && connectionIds.empty())
                continue;

            if (wanted->names (ownId.value_or (0), connectionIds))
                return endpoint;
        }

        return 0;
    }

    // CoreMIDI takes event lists of at most 65536 bytes: "Large sysex messages
    // must be sent in smaller event lists" (MIDIServices.h, MIDIEventListAdd).
    constexpr std::size_t maxEventListBytes = 65536;

    // Words given to MIDIEventListAdd per call. Even, so that a list which
    // fills up always ends between two SysEx packets (two words each).
    constexpr std::size_t wordsPerAdd = 64;
} // namespace

namespace
{
    // How often the watcher looks. An unplugged port is refused at most this
    // much later; what was handed to CoreMIDI in between is lost either way.
    constexpr auto watchPeriod = std::chrono::milliseconds (100);

    // A destination is gone when CoreMIDI lists it as offline or no longer
    // lists it at all. An unplugged USB interface does the first: its
    // endpoint stays in the list and turns offline about 0.1 s after the
    // cable is pulled (measured with the U6MIDI Pro, docs/TIMING.md). A
    // removed virtual port does the second.
    bool hasGone (MIDIEndpointRef endpoint)
    {
        const auto count = MIDIGetNumberOfDestinations();
        bool listed = false;

        for (ItemCount i = 0; i < count && ! listed; ++i)
            listed = MIDIGetDestination (i) == endpoint;

        if (! listed)
            return true;

        // Fails for an endpoint that has no such property: that one is online.
        SInt32 offline = 0;
        return MIDIObjectGetIntegerProperty (endpoint, kMIDIPropertyOffline, &offline) == noErr && offline != 0;
    }
} // namespace

CoreMidiScheduler::CoreMidiScheduler()
    : words (MidiBytesToUmp::maxWords),
      eventListStorage (maxEventListBytes / sizeof (std::uint64_t))
{
    if (MIDIClientCreate (CFSTR ("Hums"), nullptr, nullptr, &client) != noErr)
        return;

    if (MIDIOutputPortCreate (client, CFSTR ("Hums output"), &outputPort) != noErr)
    {
        outputPort = 0;
        return;
    }

    watcher = std::thread ([this] { watchPorts(); });
}

CoreMidiScheduler::~CoreMidiScheduler()
{
    if (watcher.joinable())
    {
        {
            const std::lock_guard lock (watcherWaiting);
            watcherShouldStop = true;
        }
        watcherWake.notify_one();
        watcher.join();
    }

    for (std::size_t slot = 0; slot < endpoints.size(); ++slot)
        discardPending (slot);

    if (outputPort != 0)
        MIDIPortDispose (outputPort);

    if (client != 0)
        MIDIClientDispose (client);
}

void CoreMidiScheduler::watchPorts()
{
    std::unique_lock lock (watcherWaiting);

    while (! watcherWake.wait_for (lock, watchPeriod, [this] { return watcherShouldStop; }))
    {
        lock.unlock();
        markGonePorts();
        lock.lock();
    }
}

void CoreMidiScheduler::markGonePorts()
{
    const std::lock_guard lock (slotsChanging);

    for (std::size_t slot = 0; slot < endpoints.size(); ++slot)
    {
        const auto endpoint = endpoints[slot].load (std::memory_order_relaxed);

        if (endpoint != 0 && ! gone[slot].load (std::memory_order_relaxed) && hasGone (endpoint))
            gone[slot].store (true, std::memory_order_release);
    }
}

MidiScheduler::PortHandle CoreMidiScheduler::open (const std::string& deviceIdentifier)
{
    if (! isReady())
        return invalidPort;

    const auto endpoint = findDestination (deviceIdentifier);
    if (endpoint == 0)
        return invalidPort;

    const std::lock_guard lock (slotsChanging);

    // A destination is open at most once (MidiScheduler.h): CoreMIDI cancels
    // pending messages per destination, not per handle, so two handles could
    // not be closed independently.
    for (const auto& open : endpoints)
        if (open.load (std::memory_order_relaxed) == endpoint)
            return invalidPort;

    for (int handle = 0; handle < maxPorts; ++handle)
    {
        const auto slot = static_cast<std::size_t> (handle);

        if (endpoints[slot].load (std::memory_order_relaxed) == 0)
        {
            gone[slot].store (hasGone (endpoint), std::memory_order_release);
            latestScheduled[slot].store (0, std::memory_order_release);
            endpoints[slot].store (endpoint, std::memory_order_release);
            return handle;
        }
    }

    return invalidPort;
}

void CoreMidiScheduler::close (PortHandle port)
{
    if (port < 0 || port >= maxPorts)
        return;

    const auto slot = static_cast<std::size_t> (port);
    discardPending (slot);

    const std::lock_guard lock (slotsChanging);
    endpoints[slot].store (0, std::memory_order_release);
    gone[slot].store (false, std::memory_order_release);
}

// MIDIFlushOutput unschedules everything pending for the destination,
// whichever program scheduled it, and a virtual destination is sent a System
// Reset. So it is called only when messages of this port can still be waiting
// for their time: never for a port that was only opened, one whose last
// message is already due, or one whose destination has gone.
void CoreMidiScheduler::discardPending (std::size_t slot)
{
    const auto endpoint = endpoints[slot].load (std::memory_order_acquire);
    auto& latest = latestScheduled[slot];

    if (endpoint != 0
        && ! gone[slot].load (std::memory_order_acquire)
        && latest.load (std::memory_order_acquire) > machTime.nowHost())
        MIDIFlushOutput (endpoint);

    latest.store (0, std::memory_order_release);
}

bool CoreMidiScheduler::send (PortHandle port,
                              std::span<const std::uint8_t> message,
                              std::int64_t timeNanoseconds) noexcept
{
    if (port < 0 || port >= maxPorts)
        return false;

    const auto slot = static_cast<std::size_t> (port);
    const auto endpoint = endpoints[slot].load (std::memory_order_acquire);
    if (endpoint == 0)
        return false;

    // Unplugged or removed since open(): CoreMIDI would take the message
    // without complaint and send it nowhere.
    if (gone[slot].load (std::memory_order_acquire))
        return false;

    const auto wordCount = MidiBytesToUmp::convert (message, words);
    if (wordCount == 0)
        return false;

    // Timestamp 0 means "now" to CoreMIDI; a past time is sent at once anyway.
    const MIDITimeStamp timeStamp = machTime.nanosecondsToHost (timeNanoseconds);

    // Only this thread writes it, so load-then-store is enough.
    auto& latest = latestScheduled[slot];
    if (timeStamp > latest.load (std::memory_order_relaxed))
        latest.store (timeStamp, std::memory_order_release);

    auto* list = reinterpret_cast<MIDIEventList*> (eventListStorage.data());
    auto* packet = MIDIEventListInit (list, kMIDIProtocol_1_0);

    for (std::size_t first = 0; first < wordCount; first += wordsPerAdd)
    {
        const auto count = std::min (wordsPerAdd, wordCount - first);
        auto* next = MIDIEventListAdd (list, maxEventListBytes, packet, timeStamp, count, words.data() + first);

        if (next == nullptr)
        {
            // The list is full: hand it over and continue the SysEx in a new
            // one with the same timestamp. If a later list is refused, the
            // part already handed over stays sent.
            if (MIDISendEventList (outputPort, endpoint, list) != noErr)
                return false;

            packet = MIDIEventListInit (list, kMIDIProtocol_1_0);
            next = MIDIEventListAdd (list, maxEventListBytes, packet, timeStamp, count, words.data() + first);

            if (next == nullptr)
                return false;
        }

        packet = next;
    }

    // noErr says the list was handed to CoreMIDI, not that it will arrive:
    // CoreMIDI itself reports neither a destination that has gone (hence the
    // gone flag above, which lags the unplugging by up to one watch period) nor
    // a full queue.
    return MIDISendEventList (outputPort, endpoint, list) == noErr;
}

} // namespace platform
