#pragma once

#include <cstdint>

namespace model::commands
{

// What a note is made of (docs/SCHEMA.md -> Note). As made here, it is the
// note the schema describes when only its place and pitch are stored.
struct NoteValues
{
    // The start, relative to the clip start; 0 or later. It may lie at or
    // past the loop length: such a note is kept and never played.
    std::int64_t tick = 0;
    // At least one tick, and the note must not end past the largest tick.
    std::int64_t length = 0;
    // 0 to 127.
    int pitch = 60;
    // 1 to 127.
    int velocity = 100;
    // 0 to 127.
    int offVelocity = 64;
    bool enabled = true;
};

} // namespace model::commands
