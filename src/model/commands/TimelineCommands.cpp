#include "model/commands/TimelineCommands.h"

#include "model/CanonicalDouble.h"
#include "model/Ids.h"
#include "model/MarkerModel.h"
#include "model/TempoRange.h"
#include "model/commands/NodeLookup.h"

#include <cmath>

namespace model::commands
{

namespace
{

    juce::ValueTree timelineOf (const juce::ValueTree& project)
    {
        jassert (project.hasType (ids::Project));
        return project.getChildWithName (ids::Timeline);
    }

    CommandResult noTimeline()
    {
        return CommandResult::refused ("The project has no Timeline.");
    }

    CommandResult negativeTick (std::int64_t tick)
    {
        return CommandResult::refused ("A position is tick 0 or later, not " + juce::String (tick) + ".");
    }

    juce::int64 stored (std::int64_t tick)
    {
        return static_cast<juce::int64> (tick);
    }

    bool isDenominator (int number)
    {
        return number >= 1 && number <= 64 && (number & (number - 1)) == 0;
    }

    // For a TempoEvent or a TimeSigEvent. `what` names it in the refusal.
    CommandResult removeEventAt (juce::ValueTree project, const juce::Identifier& eventType, const juce::String& what, std::int64_t tick, juce::UndoManager& undoManager)
    {
        if (tick == 0)
            return CommandResult::refused ("The " + what + " at tick 0 cannot be removed: a Timeline always has one there.");

        const auto event = NodeLookup::childWithNumber (timelineOf (project), eventType, ids::tick, tick);

        if (! event.isValid())
            return CommandResult::refused ("There is no " + what + " at tick " + juce::String (tick) + ".");

        event.getParent().removeChild (event, &undoManager);

        return CommandResult::applied();
    }

    bool isMarkerOf (const juce::ValueTree& project, const juce::ValueTree& marker)
    {
        return marker.hasType (ids::Marker) && marker.getParent().isValid() && marker.getParent() == timelineOf (project);
    }

    CommandResult notAMarker()
    {
        return CommandResult::refused ("This is not a marker of the project's Timeline.");
    }

} // namespace

CommandResult setTempo (juce::ValueTree project, std::int64_t tick, double bpm, juce::UndoManager& undoManager)
{
    auto timeline = timelineOf (project);

    if (! timeline.isValid())
        return noTimeline();

    if (tick < 0)
        return negativeTick (tick);

    const auto outOfRange = [] { return CommandResult::refused ("A tempo is " + TempoRange::inWords() + "."); };

    if (! std::isfinite (bpm))
        return outOfRange();

    // What is stored is the text that is saved, six significant digits and
    // no more [D14]. The check is on the value that text reads back as, so
    // 999.9996, which would be saved as 1000, is refused [D23].
    const auto text = CanonicalDouble::textOf (bpm);

    if (! TempoRange::contains (CanonicalDouble::rounded (bpm)))
        return outOfRange();

    auto tempoEvent = NodeLookup::childWithNumber (timeline, ids::TempoEvent, ids::tick, tick);

    if (tempoEvent.isValid())
    {
        tempoEvent.setProperty (ids::bpm, text, &undoManager);
        return CommandResult::applied();
    }

    juce::ValueTree added (ids::TempoEvent);
    added.setProperty (ids::tick, stored (tick), nullptr);
    added.setProperty (ids::bpm, text, nullptr);

    timeline.appendChild (added, &undoManager);

    return CommandResult::applied();
}

CommandResult removeTempoEvent (juce::ValueTree project, std::int64_t tick, juce::UndoManager& undoManager)
{
    return removeEventAt (project, ids::TempoEvent, "tempo", tick, undoManager);
}

CommandResult setTimeSignature (juce::ValueTree project, std::int64_t tick, int numerator, int denominator, juce::UndoManager& undoManager)
{
    auto timeline = timelineOf (project);

    if (! timeline.isValid())
        return noTimeline();

    if (tick < 0)
        return negativeTick (tick);

    if (numerator < 1 || numerator > 32)
        return CommandResult::refused ("The upper number of a time signature is from 1 to 32, not " + juce::String (numerator) + ".");

    if (! isDenominator (denominator))
        return CommandResult::refused ("The lower number of a time signature is one of 1, 2, 4, 8, 16, 32, 64, not " + juce::String (denominator) + ".");

    auto timeSigEvent = NodeLookup::childWithNumber (timeline, ids::TimeSigEvent, ids::tick, tick);

    if (timeSigEvent.isValid())
    {
        timeSigEvent.setProperty (ids::numerator, numerator, &undoManager);
        timeSigEvent.setProperty (ids::denominator, denominator, &undoManager);
        return CommandResult::applied();
    }

    juce::ValueTree added (ids::TimeSigEvent);
    added.setProperty (ids::tick, stored (tick), nullptr);
    added.setProperty (ids::numerator, numerator, nullptr);
    added.setProperty (ids::denominator, denominator, nullptr);

    timeline.appendChild (added, &undoManager);

    return CommandResult::applied();
}

CommandResult removeTimeSigEvent (juce::ValueTree project, std::int64_t tick, juce::UndoManager& undoManager)
{
    return removeEventAt (project, ids::TimeSigEvent, "time signature", tick, undoManager);
}

CommandResult addMarker (juce::ValueTree project, std::int64_t tick, const juce::String& name, juce::UndoManager& undoManager)
{
    auto timeline = timelineOf (project);

    if (! timeline.isValid())
        return noTimeline();

    if (tick < 0)
        return negativeTick (tick);

    juce::ValueTree marker (ids::Marker);
    marker.setProperty (ids::tick, stored (tick), nullptr);
    marker.setProperty (ids::name, name, nullptr);

    timeline.appendChild (marker, &undoManager);

    return CommandResult::applied();
}

CommandResult removeMarker (juce::ValueTree project, const juce::ValueTree& marker, juce::UndoManager& undoManager)
{
    if (! isMarkerOf (project, marker))
        return notAMarker();

    marker.getParent().removeChild (marker, &undoManager);

    return CommandResult::applied();
}

CommandResult moveMarker (juce::ValueTree project, const juce::ValueTree& marker, std::int64_t tick, juce::UndoManager& undoManager)
{
    if (! isMarkerOf (project, marker))
        return notAMarker();

    if (tick < 0)
        return negativeTick (tick);

    // A copy of the handle, which refers to the same node.
    auto node = marker;
    node.setProperty (ids::tick, stored (tick), &undoManager);

    return CommandResult::applied();
}

CommandResult renameMarker (juce::ValueTree project, const juce::ValueTree& marker, const juce::String& name, juce::UndoManager& undoManager)
{
    if (! isMarkerOf (project, marker))
        return notAMarker();

    // A marker that reads as this name already is left alone, so that an
    // empty name that was never stored stays unstored.
    if (MarkerModel (marker).getName() == name)
        return CommandResult::applied();

    auto node = marker;
    node.setProperty (ids::name, name, &undoManager);

    return CommandResult::applied();
}

} // namespace model::commands
