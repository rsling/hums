#pragma once

#include "RenderRun.h"
#include "render/Renderer.h"
#include "render/snapshot/Snapshot.h"

#include <span>
#include <string>
#include <vector>

namespace guardrails
{

// What the tests hold of everything they render, beyond what a golden file
// says of its one case (docs/GUARDRAILS.md -> 4. Render golden files, Beyond
// the golden files):
//
//   - a stretch of ticks gives the same events in many small windows, and
//     through a small buffer, as in one window: the engine renders one
//     look-ahead window after the other, an export everything at once, and
//     the two must not differ;
//   - the Renderer allocates nothing, because the engine thread calls it;
//   - every note-on gets its note-off, and no pitch sounds twice at once on
//     one channel of one destination.
//
// The golden test holds each of its cases to them, and the property run of
// guardrail 3 the documents of its seeds.
//
// The checks give their findings back as text and assert nothing themselves,
// so that a test can also show each of them finding what it is there to find.
// Std and the render core only.
class RenderChecks
{
public:
    // Whether a plan is also played with one tick per window, so that every
    // tick is the end of a window once. It costs a call of the Renderer per
    // tick, so it is for a plan of a few thousand ticks.
    enum class TickByTick
    {
        no,
        yes
    };

    // All three checks over one plan; empty when they hold. The plan is
    // played in one window, then in windows of a fiftieth of its length, in
    // windows of a seventh through the smallest buffer the Renderer takes,
    // and tick by tick if asked for.
    static std::vector<std::string> failuresOf (const RenderRun& run, const RenderRun::Plan& plan, TickByTick tickByTick);

    // All three checks over every plan of plansOf, those of closeReach also
    // tick by tick; the findings of the first plan that fails, with the
    // plan, and empty when all hold.
    static std::vector<std::string> failuresOf (const RenderRun& run);

    // What is played of a Snapshot that no golden file belongs to. A
    // generated document can be 2^40 ticks long and loop a clip of one tick
    // through all of them, so it is not played from end to end: the plans
    // are the ticks around every tick at which a placement of a track or of
    // an AutomationLane begins or ends, since that is where the rules meet.
    // For each such tick there are three: `reach` ticks to either side begun
    // by a seek; the same stretch through a transport loop that spans it, so
    // that it wraps at its end and is played again; and `closeReach` ticks
    // to either side begun by a seek, short enough to be played tick by tick
    // in every seed.
    static std::vector<RenderRun::Plan> plansOf (const render::Snapshot& snapshot);

    // How far the plans of plansOf reach to either side of their tick.
    static constexpr std::int64_t reach = 600;
    static constexpr std::int64_t closeReach = 120;

    // The notes of a list of events as they went out: a note-on of a pitch
    // that still sounds on its channel and destination, a note-off of one
    // that does not, and a note that still sounds at the end.
    static std::vector<std::string> noteFailuresOf (std::span<const render::Renderer::Event> events);

    // Where two lists of events differ: the first place, with both events,
    // or the lengths when one list is the beginning of the other. Empty when
    // they are the same, every member of every event compared.
    static std::vector<std::string> differenceOf (std::span<const render::Renderer::Event> inOneWindow, std::span<const render::Renderer::Event> inManyWindows);
};

} // namespace guardrails
