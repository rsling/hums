#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// One note of an event clip (docs/SCHEMA.md -> Event clip: Events): an
// object with a position and a length, not a note-on / note-off pair. It has
// no channel; the Track supplies it.
class NoteModel
{
public:
    // The node must be a Note node.
    explicit NoteModel (juce::ValueTree noteNode);

    // The stored start, relative to the clip start. With Clip.quantise set
    // the note plays elsewhere; this is the editing position.
    std::int64_t getTick() const;
    std::int64_t getLength() const;
    int getPitch() const;
    int getVelocity() const;
    // Sent as the note-off velocity.
    int getOffVelocity() const;
    // A disabled note renders nothing but stays editable.
    bool isEnabled() const;

private:
    juce::ValueTree node;
};

} // namespace model
