#pragma once

#include "model/ClipPoolModel.h"
#include "model/RoutingModel.h"
#include "model/SessionModel.h"
#include "model/Settings.h"
#include "model/TimelineModel.h"
#include "model/TracksModel.h"

#include <juce_data_structures/juce_data_structures.h>

namespace model
{

// The root of the document (docs/SCHEMA.md -> Project).
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

    // UiState, the part of the document that is not undoable. Unlike the
    // wrappers above, Settings also writes: it is the settings path.
    Settings getSettings() const;

private:
    juce::ValueTree node;
};

} // namespace model
