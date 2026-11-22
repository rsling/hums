#pragma once

#include "model/Ids.h"
#include "model/Validator.h"

#include <juce_data_structures/juce_data_structures.h>

// What the command tests start from: one small valid document and the
// UndoManager the commands are handed.
//
// Two players, one performer and two pads; a tempo and a time signature at
// tick 0 and one more of each later; two markers at one tick; three tracks,
// of which two play through the first player and one records from the
// performer; one named scene. A few values are held as numbers and most as
// text, as in a document that was loaded and then edited.
struct CommandFixture
{
    static constexpr const char* minimoogId = "9b1d6f3a2c4e4f708192a3b4c5d6e7f8";
    static constexpr const char* prophetId = "0a1b2c3d4e5f40718293a4b5c6d7e8f9";
    static constexpr const char* keyboardId = "3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1";

    juce::ValueTree project = juce::ValueTree::fromXml (
        R"(<Project schemaVersion="1" name="Command test" ppq="960">
             <Routing>
               <Player id="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" name="Minimoog" sendClock="1"/>
               <Player id="0a1b2c3d4e5f40718293a4b5c6d7e8f9" name="Prophet"/>
               <Performer id="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1" name="Keyboard"/>
               <PadMap>
                 <Pad index="0" inputPitch="36" pitch="36" label="Kick"/>
                 <Pad index="1" inputPitch="37" pitch="42"/>
               </PadMap>
             </Routing>
             <Timeline>
               <TempoEvent tick="0" bpm="118"/>
               <TimeSigEvent tick="0" numerator="4" denominator="4"/>
               <Marker tick="7680" name="B"/>
               <TempoEvent tick="7680" bpm="124.5"/>
               <TimeSigEvent tick="3840" numerator="3" denominator="4"/>
               <Marker tick="7680"/>
             </Timeline>
             <ClipPool/>
             <Tracks>
               <Track id="4c6e8a0c2e4f4b1d8a6c4e2a0c1e3f5b" name="Bass"
                      playerId="9b1d6f3a2c4e4f708192a3b4c5d6e7f8" performerId="3f2c9a1e5b7d4c80a6e1f0b2d4c6e8a1">
                 <Placements/><AutomationLanes/><Knobs/><Slots/>
               </Track>
               <Track id="5d7f9b1d3f504c2e9b7d5f3b1d2f4a6c" name="Lead" playerId="0a1b2c3d4e5f40718293a4b5c6d7e8f9">
                 <Placements/><AutomationLanes/><Knobs/><Slots/>
               </Track>
               <Track id="6e8a0c2e4a614d3fac8e6a4c2e3a5b7d" name="Bass 2" playerId="9b1d6f3a2c4e4f708192a3b4c5d6e7f8">
                 <Placements/><AutomationLanes/><Knobs/><Slots/>
               </Track>
             </Tracks>
             <Session>
               <Scene index="2" name="Chorus"/>
             </Session>
             <UiState/>
           </Project>)");

    juce::UndoManager undoManager;

    CommandFixture()
    {
        jassert (isValid());
    }

    juce::String xml() const
    {
        return project.toXmlString();
    }

    bool isValid() const
    {
        return model::Validator::validate (project).empty();
    }

    juce::ValueTree routing() const
    {
        return project.getChildWithName (model::ids::Routing);
    }

    juce::ValueTree timeline() const
    {
        return project.getChildWithName (model::ids::Timeline);
    }

    juce::ValueTree session() const
    {
        return project.getChildWithName (model::ids::Session);
    }

    // The tracks from top to bottom: Bass, Lead, Bass 2.
    juce::ValueTree track (int position) const
    {
        return project.getChildWithName (model::ids::Tracks).getChild (position);
    }

    // True when the command was refused and left everything as it was: the
    // document, and an undo history that had nothing in it.
    bool isUntouchedSince (const juce::String& xmlBefore) const
    {
        return xml() == xmlBefore && ! undoManager.canUndo() && undoManager.getNumActionsInCurrentTransaction() == 0;
    }
};
