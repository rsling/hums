#pragma once

#include <cstdint>

namespace model
{

// Where the quantise setting of an event clip puts a note onset
// (docs/SCHEMA.md -> ClipPool / Clip). The one place that says so: the
// template build of render/SnapshotBuilder asks it at playback and export,
// and applyQuantise is to ask it when it rewrites the stored ticks, so that
// what is made permanent is exactly what was heard.
//
// Grid lines are multiples of the grid measured from clip tick 0. The
// candidates are those multiples up to the loop length, and the loop length
// itself, which stands for tick 0: an onset nearer to the loop point than to
// the last line before it wraps to 0, and no line is invented past the
// loop's end. An onset goes to the nearest candidate; exactly between two it
// goes to the earlier one.
//
// Only the onset is asked here. A note keeps its length, so its end moves
// with its onset, and nothing but a note is quantised.
class Quantise
{
public:
    // The onset of a note stored at `tick`, in a clip of `loopLength` ticks
    // (> 0) whose quantise grid is `grid` ticks (ClipModel::getQuantiseGrid;
    // 0 = off). The answer is a tick of the loop, 0 <= answer < loopLength.
    //
    // With the grid off the onset is the stored tick. A note stored at or
    // past the loop length is not quantised either: it is silent either way,
    // and its tick is given back as it is.
    static std::int64_t onsetOf (std::int64_t tick, std::int64_t grid, std::int64_t loopLength);
};

} // namespace model
