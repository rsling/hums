#pragma once

#include "model/EventModel.h"
#include "model/GridModel.h"
#include "model/NoteModel.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>
#include <vector>

namespace model
{

// A unit of musical content in the ClipPool (docs/SCHEMA.md -> ClipPool /
// Clip). Tracks only reference clips, by id.
//
// An event clip holds Notes and Events; a step clip holds a Grid. The kind
// never changes: a conversion creates a new clip.
class ClipModel
{
public:
    enum class Kind
    {
        event,
        step
    };

    // The node must be a Clip node.
    explicit ClipModel (juce::ValueTree clipNode);

    juce::String getId() const;
    juce::String getName() const;
    Kind getKind() const;
    // "#AARRGGBB", or empty for none.
    juce::String getColour() const;

    // The length of one pass of the loop, in ticks. An event clip stores it,
    // and its content may be shorter or longer. A step clip does not store
    // it: it follows from the Grid (step length x audible columns).
    std::int64_t getLoopLength() const;

    // Event clips only. The playback quantisation of note onsets as stored:
    // "" (off), "1/2", "1/4", "1/8", "1/2T", "1/4T" or "1/8T".
    juce::String getQuantise() const;
    // The same as a grid in ticks, measured from clip tick 0; 0 = off.
    std::int64_t getQuantiseGrid() const;

    // Event clips only; a step clip has none. In child order, which is not
    // tick order: it is the tie-breaker at equal ticks.
    std::vector<NoteModel> getNotes() const;
    // Event clips only; a step clip has none. In child order, likewise.
    std::vector<EventModel> getEvents() const;

    // Step clips only: an event clip has no Grid, and asking for it asserts.
    GridModel getGrid() const;

private:
    juce::ValueTree node;
};

} // namespace model
