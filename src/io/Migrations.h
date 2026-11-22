#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <functional>
#include <vector>

namespace io
{

// The steps that bring a project file of an older schemaVersion up to the
// current one (docs/SCHEMA.md -> Migrations).
//
// The schema is at version 1 and nothing has shipped, so the table is empty.
// The first format change adds migrate1to2 to Migrations.cpp and its line to
// the table, with a test and a fixture file of the old version.
class Migrations
{
public:
    // One step: it rewrites a detached candidate tree of one version into the
    // next. It runs before the Validator, so it may meet anything a file of
    // its version can hold. A step that cannot do its work throws (a
    // std::exception, whose text is reported); the load is then refused and
    // nothing but the candidate has been touched. The step leaves
    // schemaVersion alone: the load path sets it after each step.
    using Step = std::function<void (juce::ValueTree& candidate)>;

    // In order: the first step takes version 1 to 2, the second 2 to 3, and
    // so on, without gaps. The table therefore also says which version a
    // build writes and reads: see versionReachedBy.
    using Steps = std::vector<Step>;

    // The steps of this build.
    static const Steps& steps();

    // The schemaVersion a document has after the last of these steps: 1 with
    // an empty table, one more for every step.
    static int versionReachedBy (const Steps& steps);

    // The schemaVersion this build writes, and the highest it reads.
    static int currentVersion();
};

} // namespace io
