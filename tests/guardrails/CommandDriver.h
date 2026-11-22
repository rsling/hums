#pragma once

#include "CommandChecks.h"
#include "RandomSource.h"
#include "model/commands/CommandResult.h"

#include <juce_data_structures/juce_data_structures.h>

#include <cstdint>
#include <string>
#include <vector>

namespace guardrails
{

// Applies random commands of model/commands/ to a document, for the property
// tests (docs/GUARDRAILS.md -> 3): valid ones and deliberately invalid ones
// (a port that is not there, a pad number that is taken, a tempo of 0, the
// tempo at tick 0 removed, a track on channel 17, a clip removed that is
// still placed).
//
// It looks at the document as it is now to know what is valid: which ids
// there are, which pad numbers are taken. A valid form that needs something
// the document does not have (a Performer to rename, when there is none)
// gives way to an invalid form.
//
// Every command of model/commands/ belongs in the table in the .cpp file,
// with the task that adds it; a test holds each of them to be reached, in
// its valid form and, where it has one, in an invalid form.
class CommandDriver
{
public:
    // What a test needs to know of one command of the table.
    struct KnownCommand
    {
        std::string name;

        // False for the few commands that nothing can make invalid
        // (addPerformer takes any name).
        bool hasInvalidForm = true;
    };

    static std::vector<KnownCommand> knownCommands();

    // `project` is a valid document; `undoManager` and `random` must outlive
    // the driver.
    CommandDriver (juce::ValueTree project, juce::UndoManager& undoManager, RandomSource& random);

    // Applies one command, in a valid or an invalid form. The description of
    // the attempt begins with the command's name.
    CommandChecks::Attempt attemptOne();

private:
    using Attempt = CommandChecks::Attempt;

    Attempt made (const juce::String& call, bool isValidForm, const model::commands::CommandResult& result) const;

    // Values.
    bool wantsValidForm();
    juce::String text();
    juce::String unknownId();
    std::int64_t tick();
    std::int64_t negativeTick();
    double tempo();
    double noTempo();
    int notAPitch();

    // What the document holds now.
    std::vector<juce::String> idsOf (const juce::Identifier& portType) const;
    std::vector<juce::ValueTree> pads() const;
    std::vector<std::int64_t> ticksOf (const juce::Identifier& timelineEventType) const;
    std::vector<juce::ValueTree> markers() const;
    juce::ValueTree noMarker();
    std::vector<juce::ValueTree> tracks() const;
    std::vector<juce::ValueTree> tracksWithKnobs() const;
    juce::String colour();
    juce::String noColour();
    std::vector<juce::ValueTree> clips() const;
    int refcountOf (const juce::ValueTree& clip) const;

    // Routing.
    Attempt addPerformer();
    Attempt removePerformer();
    Attempt setPerformerName();
    Attempt setPerformerDevice();
    Attempt setPerformerChannelFilter();
    Attempt addPlayer();
    Attempt removePlayer();
    Attempt setPlayerName();
    Attempt setPlayerDevice();
    Attempt setPlayerSendClock();

    // PadMap.
    Attempt addPad();
    Attempt removePad();
    Attempt setPadInputPitch();
    Attempt setPadPitch();
    Attempt setPadLabel();

    // Timeline.
    Attempt setTempo();
    Attempt removeTempoEvent();
    Attempt setTimeSignature();
    Attempt removeTimeSigEvent();
    Attempt addMarker();
    Attempt removeMarker();
    Attempt moveMarker();
    Attempt renameMarker();

    // Session.
    Attempt setLaunchQuantise();
    Attempt setSceneName();

    // Tracks and their knobs.
    Attempt addTrack();
    Attempt removeTrack();
    Attempt moveTrack();
    Attempt setTrackName();
    Attempt setTrackColour();
    Attempt setTrackPlayer();
    Attempt setTrackChannel();
    Attempt setTrackPerformer();
    Attempt setTrackMute();
    Attempt setTrackSolo();
    Attempt addKnob();
    Attempt removeKnob();
    Attempt moveKnob();
    Attempt setKnobCc();
    Attempt setKnobLabel();

    // The clip pool.
    Attempt addEventClip();
    Attempt addStepClip();
    Attempt setClipName();
    Attempt setClipColour();
    Attempt removeClip();
    Attempt removeUnusedClips();

    // A setter of one Track property that names a port, and of one that is
    // a flag.
    Attempt setTrackPort (const char* name, const juce::Identifier& portType, model::commands::CommandResult (*command) (juce::ValueTree, const juce::String&, const juce::String&, juce::UndoManager&));
    Attempt setTrackFlag (const char* name, model::commands::CommandResult (*command) (juce::ValueTree, const juce::String&, bool, juce::UndoManager&));

    struct TableEntry
    {
        const char* name;
        Attempt (CommandDriver::*attempt)();
        bool hasInvalidForm;
    };

    static const std::vector<TableEntry>& table();

    juce::ValueTree project;
    juce::UndoManager& undoManager;
    RandomSource& random;
};

} // namespace guardrails
