#pragma once

#include "model/ClipPoolModel.h"
#include "model/RoutingModel.h"
#include "model/SessionModel.h"
#include "model/TimelineModel.h"
#include "model/TracksModel.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// The root of the document (docs/SCHEMA.md -> Project).
//
// UiState gets its accessor together with model/Settings.
class ProjectModel
{
public:
    // The node must be a Project node.
    explicit ProjectModel (juce::ValueTree projectNode);

    int getSchemaVersion() const;
    juce::String getName() const;
    // Self-description only: the code assumes 960 and the Validator refuses
    // any other value.
    int getPpq() const;

    RoutingModel getRouting() const;
    TimelineModel getTimeline() const;
    ClipPoolModel getClipPool() const;
    TracksModel getTracks() const;
    SessionModel getSession() const;

private:
    juce::ValueTree node;
};

} // namespace model
