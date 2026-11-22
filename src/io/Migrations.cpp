#include "io/Migrations.h"

namespace io
{

// One function per step goes here, named for the versions it joins
// (migrate1to2, ...), each with its line in the table below. This file is the
// one place under src/ that may spell a type or property name as a string
// (docs/GUARDRAILS.md -> 1): a step has to name what Ids.h no longer declares.

const Migrations::Steps& Migrations::steps()
{
    // Empty: no format change has been made since version 1.
    static const Steps table {};

    return table;
}

int Migrations::versionReachedBy (const Steps& steps)
{
    return 1 + static_cast<int> (steps.size());
}

int Migrations::currentVersion()
{
    return versionReachedBy (steps());
}

} // namespace io
