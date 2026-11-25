#include "RenderChecks.h"

#include "render/snapshot/TrackPlan.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <set>

namespace guardrails
{

namespace
{

    using Event = render::Renderer::Event;

    // The buffer of the run in one window: more than the Renderer asks the
    // Kernel for at a time.
    const std::size_t largeBuffer = 256;

    // The smallest buffer the Renderer takes: a note-on and the off before it.
    const std::size_t smallestBuffer = 2;

    std::string kindOf (render::Renderer::EventKind kind)
    {
        switch (kind)
        {
            case render::Renderer::EventKind::noteOff:
                return "noteOff";
            case render::Renderer::EventKind::noteOn:
                return "noteOn";
            case render::Renderer::EventKind::cc:
                return "cc";
            case render::Renderer::EventKind::pitchBend:
                return "pitchBend";
            case render::Renderer::EventKind::programChange:
                return "programChange";
            case render::Renderer::EventKind::channelPressure:
                return "channelPressure";
            case render::Renderer::EventKind::polyPressure:
                return "polyPressure";
            case render::Renderer::EventKind::sysex:
                return "sysex";
        }

        return "unknown";
    }

    std::string textOf (const Event& event)
    {
        std::string text = "tick " + std::to_string (event.tick) + " track " + std::to_string (event.track) + " destination "
                         + std::to_string (event.destination) + " channel " + std::to_string (event.channel) + " " + kindOf (event.kind) + " "
                         + std::to_string (event.a) + " " + std::to_string (event.b);

        if (event.kind == render::Renderer::EventKind::sysex)
            text += " payload " + std::to_string (event.dataLength) + " bytes at " + std::to_string (event.dataStart) + " of template "
                  + std::to_string (event.templateIndex);

        return text;
    }

    bool areSame (const Event& one, const Event& other)
    {
        return one.tick == other.tick && one.track == other.track && one.destination == other.destination && one.channel == other.channel
            && one.kind == other.kind && one.a == other.a && one.b == other.b && one.templateIndex == other.templateIndex
            && one.dataStart == other.dataStart && one.dataLength == other.dataLength;
    }

    std::string textOf (const RenderRun::Plan& plan)
    {
        std::string text = "ticks " + std::to_string (plan.start) + " to " + std::to_string (plan.end);

        if (plan.beginsWithSeek)
            text += ", begun by a seek";

        if (plan.loopStart.has_value() && plan.loopEnd.has_value())
            text += ", transport loop " + std::to_string (*plan.loopStart) + " to " + std::to_string (*plan.loopEnd);

        return text;
    }

    // The longest stretch of ticks the plan plays in one go.
    std::int64_t longestStretchOf (const RenderRun::Plan& plan)
    {
        if (plan.loopStart.has_value() && plan.loopEnd.has_value())
            return std::max (*plan.loopEnd - plan.start, plan.end - *plan.loopStart);

        return plan.end - plan.start;
    }

    void addStartAndEnd (std::set<std::int64_t>& ticks, const std::vector<render::TrackPlan::Placement>& placements)
    {
        for (const auto& placement : placements)
        {
            ticks.insert (placement.start);
            ticks.insert (placement.start + placement.length);
        }
    }

} // namespace

std::vector<std::string> RenderChecks::failuresOf (const RenderRun& run, const RenderRun::Plan& plan, TickByTick tickByTick)
{
    std::vector<std::string> failures;

    const auto inOneWindow = run.play (plan, RenderRun::oneWindow, largeBuffer);
    std::size_t allocationCount = inOneWindow.allocationCount;

    for (const auto& failure : noteFailuresOf (inOneWindow.events))
        failures.push_back (failure);

    // Windows of a fiftieth and of a seventh of the stretch, so that the
    // ends of the windows fall on other ticks each time, the second through
    // the smallest buffer; and tick by tick where that is asked for.
    struct Windowing
    {
        std::int64_t windowLength = 0;
        std::size_t bufferSize = 0;
    };

    const std::int64_t stretch = longestStretchOf (plan);
    std::vector<Windowing> windowings { { stretch / 50 + 1, largeBuffer }, { stretch / 7 + 1, smallestBuffer } };

    if (tickByTick == TickByTick::yes)
        windowings.push_back ({ 1, largeBuffer });

    for (const auto& windowing : windowings)
    {
        const auto inManyWindows = run.play (plan, windowing.windowLength, windowing.bufferSize);
        allocationCount += inManyWindows.allocationCount;

        const auto difference = differenceOf (inOneWindow.events, inManyWindows.events);

        if (! difference.empty())
        {
            failures.push_back ("in windows of " + std::to_string (windowing.windowLength) + " ticks through a buffer of "
                                + std::to_string (windowing.bufferSize) + " events the render gives other events than in one window:");
            failures.insert (failures.end(), difference.begin(), difference.end());
        }
    }

    if (allocationCount > 0)
        failures.push_back ("the Renderer allocated " + std::to_string (allocationCount) + " times while it played");

    return failures;
}

std::vector<std::string> RenderChecks::failuresOf (const RenderRun& run)
{
    for (const auto& plan : plansOf (run.getSnapshot()))
    {
        // The close plans are the short ones.
        const bool isClose = plan.end - plan.start <= 2 * closeReach;
        auto failures = failuresOf (run, plan, isClose ? TickByTick::yes : TickByTick::no);

        if (! failures.empty())
        {
            failures.insert (failures.begin(), "rendering " + textOf (plan) + ":");
            return failures;
        }
    }

    return {};
}

std::vector<RenderRun::Plan> RenderChecks::plansOf (const render::Snapshot& snapshot)
{
    std::set<std::int64_t> ticks;

    for (const auto& trackPlan : snapshot.trackPlans)
    {
        addStartAndEnd (ticks, trackPlan.placements);

        for (const auto& lane : trackPlan.automationLanes)
            addStartAndEnd (ticks, lane.placements);
    }

    std::vector<RenderRun::Plan> plans;

    for (const std::int64_t tick : ticks)
    {
        RenderRun::Plan begunBySeek;
        begunBySeek.start = std::max (std::int64_t { 0 }, tick - reach);
        begunBySeek.end = tick + reach;
        begunBySeek.beginsWithSeek = true;
        plans.push_back (begunBySeek);

        RenderRun::Plan throughLoop;
        throughLoop.start = begunBySeek.start;
        throughLoop.end = begunBySeek.end;
        throughLoop.loopStart = throughLoop.start;
        throughLoop.loopEnd = throughLoop.end;
        plans.push_back (throughLoop);

        RenderRun::Plan close;
        close.start = std::max (std::int64_t { 0 }, tick - closeReach);
        close.end = tick + closeReach;
        close.beginsWithSeek = true;
        plans.push_back (close);
    }

    return plans;
}

std::vector<std::string> RenderChecks::noteFailuresOf (std::span<const render::Renderer::Event> events)
{
    std::vector<std::string> failures;

    // The notes that sound: destination, channel, pitch.
    struct Sounding
    {
        std::size_t destination = 0;
        int channel = 0;
        int pitch = 0;

        bool operator< (const Sounding& other) const
        {
            if (destination != other.destination)
                return destination < other.destination;

            if (channel != other.channel)
                return channel < other.channel;

            return pitch < other.pitch;
        }
    };

    std::set<Sounding> sounding;

    for (const auto& event : events)
    {
        const Sounding note { event.destination, event.channel, event.a };

        if (event.kind == render::Renderer::EventKind::noteOn)
        {
            if (! sounding.insert (note).second)
                failures.push_back ("a note-on of a pitch that still sounds: " + textOf (event));
        }
        else if (event.kind == render::Renderer::EventKind::noteOff)
        {
            if (sounding.erase (note) == 0)
                failures.push_back ("a note-off of a pitch that does not sound: " + textOf (event));
        }
    }

    for (const auto& note : sounding)
        failures.push_back ("a note-on without a note-off: destination " + std::to_string (note.destination) + " channel "
                            + std::to_string (note.channel) + " pitch " + std::to_string (note.pitch));

    return failures;
}

std::vector<std::string> RenderChecks::differenceOf (std::span<const render::Renderer::Event> inOneWindow, std::span<const render::Renderer::Event> inManyWindows)
{
    const std::size_t sharedCount = std::min (inOneWindow.size(), inManyWindows.size());

    for (std::size_t index = 0; index < sharedCount; ++index)
        if (! areSame (inOneWindow[index], inManyWindows[index]))
            return { "event " + std::to_string (index) + " in one window:   " + textOf (inOneWindow[index]),
                     "event " + std::to_string (index) + " in many windows: " + textOf (inManyWindows[index]) };

    if (inOneWindow.size() != inManyWindows.size())
        return { std::to_string (inOneWindow.size()) + " events in one window, " + std::to_string (inManyWindows.size())
                 + " in many windows; the shorter list is the beginning of the longer" };

    return {};
}

} // namespace guardrails
