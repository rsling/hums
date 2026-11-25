// guardrails::RenderChecks and guardrails::RenderRun: what the golden tests
// and the property tests hold of everything they render (docs/GUARDRAILS.md
// -> 4, Beyond the golden files). Each check is shown to find what it is
// there to find; a check that finds nothing in anything would let every
// render pass.

#include <catch2/catch_test_macros.hpp>

#include "RenderChecks.h"
#include "RenderRun.h"
#include "render/Renderer.h"
#include "render/TempoMap.h"
#include "render/snapshot/ClipTemplate.h"
#include "render/snapshot/Snapshot.h"
#include "render/snapshot/TrackPlan.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace
{

using guardrails::RenderChecks;
using guardrails::RenderRun;
using Event = render::Renderer::Event;

Event noteOnAt (std::int64_t tick, int pitch)
{
    Event event;
    event.tick = tick;
    event.kind = render::Renderer::EventKind::noteOn;
    event.a = pitch;
    event.b = 100;
    return event;
}

Event noteOffAt (std::int64_t tick, int pitch)
{
    Event event;
    event.tick = tick;
    event.kind = render::Renderer::EventKind::noteOff;
    event.a = pitch;
    event.b = 64;
    return event;
}

bool anyContains (const std::vector<std::string>& failures, const std::string& text)
{
    for (const auto& failure : failures)
        if (failure.find (text) != std::string::npos)
            return true;

    return false;
}

// One track on channel 1 with a clip of 960 ticks from tick 1000 to 2920,
// twice through its loop: a note that is cut by the loop's second pass
// beginning it again, and a controller.
render::Snapshot smallSnapshot()
{
    render::ClipTemplate clipTemplate;
    clipTemplate.loopLength = 960;
    clipTemplate.notes.push_back ({ 0, 1200, 60, 100, 64 });
    clipTemplate.notes.push_back ({ 480, 100, 64, 100, 64 });
    clipTemplate.events.push_back ({ 240, render::ClipTemplate::EventKind::cc, 7, 90, 0, 0 });
    clipTemplate.controllerChases.push_back ({ 7, { { 240, 90 } } });
    clipTemplate.controllers.set (7);

    render::TrackPlan trackPlan;
    trackPlan.channel = 1;
    trackPlan.placements.push_back ({ 1000, 1920, 0, 0 });

    return render::Snapshot { render::TempoMap ({ { 0, 500000 } }), { clipTemplate }, { trackPlan } };
}

} // namespace

TEST_CASE ("guardrail 4: a render that holds passes the render checks", "[guardrails][RenderChecks]")
{
    const auto snapshot = smallSnapshot();
    const RenderRun run (snapshot, { 0 }, 1);

    RenderRun::Plan plan;
    plan.start = 0;
    plan.end = 4000;

    // Not an empty render that passes for want of anything to look at.
    const auto result = run.play (plan, RenderRun::oneWindow, 64);
    CHECK (result.events.size() == 10);
    CHECK (result.allocationCount == 0);

    CHECK (RenderChecks::failuresOf (run, plan, RenderChecks::TickByTick::no).empty());
    CHECK (RenderChecks::failuresOf (run, plan, RenderChecks::TickByTick::yes).empty());
    CHECK (RenderChecks::failuresOf (run).empty());
}

TEST_CASE ("guardrail 4: the render run plays a seek and the wrap of a loop as its plan says", "[guardrails][RenderRun]")
{
    const auto snapshot = smallSnapshot();
    const RenderRun run (snapshot, { 0 }, 1);

    // Begun by a seek behind the controller: its value is chased, and the
    // note that began before the seek is not brought in.
    RenderRun::Plan begunBySeek;
    begunBySeek.start = 1300;
    begunBySeek.end = 1400;
    begunBySeek.beginsWithSeek = true;

    const auto chased = run.play (begunBySeek, RenderRun::oneWindow, 64);
    REQUIRE (chased.events.size() == 1);
    CHECK (chased.events[0].tick == 1300);
    CHECK (chased.events[0].kind == render::Renderer::EventKind::cc);
    CHECK (chased.events[0].b == 90);

    // Through a loop: the note is ended at the loop's end, and after the
    // wrap it begins again.
    RenderRun::Plan throughLoop;
    throughLoop.start = 1000;
    throughLoop.end = 1100;
    throughLoop.loopStart = 1000;
    throughLoop.loopEnd = 1100;

    const auto wrapped = run.play (throughLoop, RenderRun::oneWindow, 64);
    REQUIRE (wrapped.events.size() == 4);
    CHECK (wrapped.events[0].kind == render::Renderer::EventKind::noteOn);
    CHECK (wrapped.events[0].tick == 1000);
    CHECK (wrapped.events[1].kind == render::Renderer::EventKind::noteOff);
    CHECK (wrapped.events[1].tick == 1100);
    CHECK (wrapped.events[2].kind == render::Renderer::EventKind::noteOn);
    CHECK (wrapped.events[2].tick == 1000);
    CHECK (wrapped.events[3].kind == render::Renderer::EventKind::noteOff);
    CHECK (wrapped.events[3].tick == 1100);
}

TEST_CASE ("guardrail 4: the plans of a snapshot are the ticks around every start and end of a placement: begun by a seek, through a loop, and close by", "[guardrails][RenderChecks]")
{
    auto snapshot = smallSnapshot();

    // A lane placement that ends where the track's placement ends, and
    // begins near tick 0.
    render::TrackPlan::AutomationLane lane;
    lane.placements.push_back ({ 100, 2820, 0, 0 });
    snapshot.trackPlans[0].automationLanes.push_back (lane);

    const auto plans = RenderChecks::plansOf (snapshot);
    const auto reach = RenderChecks::reach;
    const auto closeReach = RenderChecks::closeReach;

    // Three ticks: 100, 1000 and 2920, which the two placements share.
    REQUIRE (plans.size() == 9);

    CHECK (plans[0].start == 0);
    CHECK (plans[0].end == 100 + reach);
    CHECK (plans[0].beginsWithSeek);
    CHECK (! plans[0].loopStart.has_value());

    CHECK (plans[1].start == 0);
    CHECK (plans[1].end == 100 + reach);
    CHECK (! plans[1].beginsWithSeek);
    CHECK (plans[1].loopStart == 0);
    CHECK (plans[1].loopEnd == 100 + reach);

    CHECK (plans[2].start == 0);
    CHECK (plans[2].end == 100 + closeReach);
    CHECK (plans[2].beginsWithSeek);
    CHECK (! plans[2].loopStart.has_value());

    CHECK (plans[3].start == 1000 - reach);
    CHECK (plans[3].end == 1000 + reach);
    CHECK (plans[5].start == 1000 - closeReach);
    CHECK (plans[5].end == 1000 + closeReach);
    CHECK (plans[6].start == 2920 - reach);
    CHECK (plans[7].loopEnd == 2920 + reach);

    CHECK (RenderChecks::plansOf (render::Snapshot { render::TempoMap ({ { 0, 500000 } }), {}, {} }).empty());
}

TEST_CASE ("guardrail 4: a note-on without a note-off is found", "[guardrails][RenderChecks]")
{
    const std::vector<Event> events { noteOnAt (0, 60), noteOnAt (0, 64), noteOffAt (100, 60) };
    const auto failures = RenderChecks::noteFailuresOf (events);

    REQUIRE (failures.size() == 1);
    CHECK (failures.front() == "a note-on without a note-off: destination 0 channel 1 pitch 64");
}

TEST_CASE ("guardrail 4: a note-on of a pitch that still sounds, and a note-off of one that does not, are found", "[guardrails][RenderChecks]")
{
    const std::vector<Event> twice { noteOnAt (0, 60), noteOnAt (50, 60), noteOffAt (100, 60) };
    const auto soundingTwice = RenderChecks::noteFailuresOf (twice);

    REQUIRE (soundingTwice.size() == 1);
    CHECK (anyContains (soundingTwice, "a note-on of a pitch that still sounds: tick 50"));

    const std::vector<Event> orphan { noteOnAt (0, 60), noteOffAt (100, 60), noteOffAt (100, 60) };
    const auto orphanOff = RenderChecks::noteFailuresOf (orphan);

    REQUIRE (orphanOff.size() == 1);
    CHECK (anyContains (orphanOff, "a note-off of a pitch that does not sound: tick 100"));
}

TEST_CASE ("guardrail 4: the same pitch on another channel or another destination is another note", "[guardrails][RenderChecks]")
{
    auto otherChannel = noteOnAt (0, 60);
    otherChannel.channel = 2;
    auto otherChannelOff = noteOffAt (100, 60);
    otherChannelOff.channel = 2;

    auto otherDestination = noteOnAt (0, 60);
    otherDestination.destination = 1;
    auto otherDestinationOff = noteOffAt (100, 60);
    otherDestinationOff.destination = 1;

    const std::vector<Event> events { noteOnAt (0, 60), otherChannel, otherDestination, noteOffAt (100, 60), otherChannelOff, otherDestinationOff };

    CHECK (RenderChecks::noteFailuresOf (events).empty());
}

TEST_CASE ("guardrail 4: two renders that differ are found: in an event, in the order, and in the number of events", "[guardrails][RenderChecks]")
{
    const std::vector<Event> inOneWindow { noteOnAt (0, 60), noteOnAt (0, 64), noteOffAt (100, 60), noteOffAt (100, 64) };

    CHECK (RenderChecks::differenceOf (inOneWindow, inOneWindow).empty());

    auto otherValue = inOneWindow;
    otherValue[2].b = 0;
    const auto valueFailures = RenderChecks::differenceOf (inOneWindow, otherValue);
    REQUIRE (valueFailures.size() == 2);
    CHECK (valueFailures[0] == "event 2 in one window:   tick 100 track 0 destination 0 channel 1 noteOff 60 64");
    CHECK (valueFailures[1] == "event 2 in many windows: tick 100 track 0 destination 0 channel 1 noteOff 60 0");

    // The same events in another order are another render: the order within
    // a tick is part of what goes out.
    const std::vector<Event> otherOrder { noteOnAt (0, 64), noteOnAt (0, 60), noteOffAt (100, 60), noteOffAt (100, 64) };
    CHECK (anyContains (RenderChecks::differenceOf (inOneWindow, otherOrder), "event 0 in many windows"));

    auto oneLess = inOneWindow;
    oneLess.pop_back();
    const auto countFailures = RenderChecks::differenceOf (inOneWindow, oneLess);
    REQUIRE (countFailures.size() == 1);
    CHECK (anyContains (countFailures, "4 events in one window, 3 in many windows"));

    // A member that only a sysex event uses counts too.
    auto otherPayload = inOneWindow;
    otherPayload[0].dataLength = 3;
    CHECK (! RenderChecks::differenceOf (inOneWindow, otherPayload).empty());
}
