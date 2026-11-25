#pragma once

#include "render/Renderer.h"
#include "render/snapshot/Snapshot.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace guardrails
{

// Plays a Snapshot through render::Renderer and keeps what goes out
// (docs/GUARDRAILS.md -> 4. Render golden files). The one such loop of the
// tests that render documents: the golden test plays its cases with it, and
// guardrails::RenderChecks plays everything a second time in small windows.
//
// Every track is heard. While it plays, it counts what the Renderer takes
// from the heap (guardrails::AllocationCounter), around the Renderer's calls
// alone: keeping the events is the test's own business.
//
// Std and the render core only.
class RenderRun
{
public:
    // What is played, as the header of a golden case says it.
    struct Plan
    {
        // The ticks that are played, the end not included.
        std::int64_t start = 0;
        std::int64_t end = 0;

        // Whether playback begins at `start` by a seek, so that the
        // controllers are chased for it first.
        bool beginsWithSeek = false;

        // The transport loop, both or neither: playback goes from `start` to
        // the loop's end, wraps there once (Renderer::seek to the loop's
        // start) and goes on from the loop's start to `end`.
        std::optional<std::int64_t> loopStart;
        std::optional<std::int64_t> loopEnd;
    };

    struct Result
    {
        // Everything that went out, in the order in which it did. The last
        // of it are the offs of the notes that still sounded at the end.
        std::vector<render::Renderer::Event> events;

        // How often the Renderer allocated; 0 where nothing is counted.
        std::size_t allocationCount = 0;
    };

    // For `windowLength`: each stretch of ticks in one window.
    static constexpr std::int64_t oneWindow = 0;

    // destinationOfTrack and destinationCount are what the Renderer and the
    // NoteTracker take. The Snapshot must outlive the run.
    RenderRun (const render::Snapshot& snapshotToPlay, std::vector<std::size_t> destinationOfTrackToUse, std::size_t destinationCountToUse);

    // Plays the plan: the seek if it begins with one, the ticks in windows
    // of `windowLength` through a buffer of `bufferSize` events (2 at
    // least), the wrap of the loop if it has one, and at the end the stop.
    // Each call begins with nothing sounding.
    Result play (const Plan& plan, std::int64_t windowLength, std::size_t bufferSize) const;

    const render::Snapshot& getSnapshot() const noexcept;

private:
    const render::Snapshot& snapshot;
    std::vector<std::size_t> destinationOfTrack;
    std::size_t destinationCount = 0;
    std::vector<bool> allTracks;
};

} // namespace guardrails
