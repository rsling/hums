#pragma once

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>

namespace model
{

// The settings path: the one way to read and write the properties of UiState
// (docs/SCHEMA.md -> UiState).
//
// UiState is the part of the document that is not undoable, so this is one of
// the two deliberate exceptions to the command path (CLAUDE.md ->
// Conventions): every write here passes a nullptr UndoManager, and no setter
// takes one. Undo never brings a setting back, and a setting never clears the
// redo history.
//
// A setter that is handed the value the setting already has writes nothing,
// so no listener hears of it: "changed" means the value changed. That is what
// lets a listener on UiState mark the project dirty.
//
// A view's own child node under UiState (zoom, scroll, visible CC rows) is
// not handled here; it arrives with the first view.
class Settings
{
public:
    // The refcount gate's current setting (docs/EDITING.md).
    enum class SharedEditPolicy
    {
        updateAll,
        makeUnique
    };

    // The transport loop, in arrangement ticks: [start, end).
    struct TransportLoop
    {
        std::int64_t start = 0;
        std::int64_t end = 0;
        bool enabled = false;
    };

    // The node must be a UiState node.
    explicit Settings (juce::ValueTree uiStateNode);

    // The tab that was showing; "arrangement" when none is stored. The schema
    // does not list the tab names, so any text is kept as it is.
    juce::String getActiveTab() const;
    void setActiveTab (const juce::String& tab);

    SharedEditPolicy getSharedEditPolicy() const;
    void setSharedEditPolicy (SharedEditPolicy policy);

    TransportLoop getTransportLoop() const;
    // Sets start, end and enabled together. Refused, with nothing written,
    // when start or end is negative, or when the loop is enabled and start is
    // not before end. Returns whether the loop was accepted.
    //
    // The loop reaches the engine as a transport command sent from here, not
    // through the Snapshot; that forwarding is built with the engine
    // (milestone 3).
    bool setTransportLoop (const TransportLoop& loop);

    // The render length for SMF export in ticks; 0 = the arrangement end.
    std::int64_t getExportLength() const;
    // Refused, with nothing written, when negative. Returns whether the
    // length was accepted.
    bool setExportLength (std::int64_t ticks);

private:
    void writeTick (const juce::Identifier& property, std::int64_t valueWhenAbsent, std::int64_t ticks);
    void writeLoopEnabled (bool enabled);

    juce::ValueTree node;
};

} // namespace model
