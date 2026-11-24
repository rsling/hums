#pragma once

namespace model::commands
{

// What a cell is made of, its CC values aside (docs/SCHEMA.md -> Cell). As
// made here, it is the cell the schema describes when nothing is stored: a
// rest.
struct CellValues
{
    // 0 to 127. Kept, and not played, in a lane with a fixed pitch.
    int pitch = 60;
    // An off cell is a rest.
    bool on = false;
    // 1 to 127.
    int velocity = 100;
    // The note's length as a fraction of the step length: greater than 0
    // and at most 4. The document keeps six significant digits [D14]; the
    // range is asked of the value so rounded.
    double gate = 0.5;
};

} // namespace model::commands
