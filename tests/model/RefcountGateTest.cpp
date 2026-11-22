// model::commands::RefcountGate: which clip a change lands on when the clip
// is referenced more than once (docs/EDITING.md). The changes here are the
// clip's name and colour, the first commands that pass the gate.

#include <catch2/catch_test_macros.hpp>

#include "CommandFixture.h"
#include "model/ClipModel.h"
#include "model/ClipPoolModel.h"
#include "model/PlacementModel.h"
#include "model/TrackModel.h"
#include "model/TracksModel.h"
#include "model/commands/AutomationLaneCommands.h"
#include "model/commands/ClipPoolCommands.h"
#include "model/commands/EditContext.h"
#include "model/commands/NodeLookup.h"
#include "model/commands/PlacementCommands.h"
#include "model/commands/RefcountGate.h"
#include "model/commands/SlotCommands.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace
{

namespace ids = model::ids;
namespace commands = model::commands;
using commands::EditContext;
using commands::RefcountGate;
using Policy = model::Settings::SharedEditPolicy;

const char* const unknownId = "ffffffffffffffffffffffffffffffff";

std::vector<model::ClipModel> clipsOf (const CommandFixture& fixture)
{
    return model::ClipPoolModel (fixture.project.getChildWithName (ids::ClipPool)).getClips();
}

// The names of the clips in pool order, as one text.
std::string clipNamesOf (const CommandFixture& fixture)
{
    std::string names;

    for (const auto& clip : clipsOf (fixture))
        names += clip.getName().toStdString() + "|";

    return names;
}

juce::ValueTree clipNode (const CommandFixture& fixture, const juce::String& clipId)
{
    return commands::NodeLookup::childWithId (fixture.project.getChildWithName (ids::ClipPool), ids::Clip, clipId);
}

int refcountOf (const CommandFixture& fixture, const juce::String& clipId)
{
    return model::TracksModel (fixture.project.getChildWithName (ids::Tracks)).getRefcount (clipId);
}

juce::String trackId (const CommandFixture& fixture, int trackPosition)
{
    return model::TrackModel (fixture.track (trackPosition)).getId();
}

juce::String addedEventClip (CommandFixture& fixture, const juce::String& name)
{
    const auto result = commands::addEventClip (fixture.project, name, 3840, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

// A placement of 960 ticks on that track; the result is its id.
juce::String placeClip (CommandFixture& fixture, int trackPosition, const juce::String& clipId, std::int64_t start)
{
    const auto result = commands::addPlacement (fixture.project, trackId (fixture, trackPosition), clipId, start, 960, fixture.undoManager);
    REQUIRE (result.wasApplied());
    return result.createdId;
}

void putClipInSlot (CommandFixture& fixture, int trackPosition, int scene, const juce::String& clipId)
{
    REQUIRE (commands::setSlot (fixture.project, trackId (fixture, trackPosition), scene, clipId, fixture.undoManager).wasApplied());
}

juce::String clipIdOfPlacement (const CommandFixture& fixture, const juce::String& placementId)
{
    return model::PlacementModel (commands::NodeLookup::placementWithId (fixture.project, placementId)).getClipId();
}

juce::String clipIdInSlot (const CommandFixture& fixture, int trackPosition, int scene)
{
    for (const auto& slot : model::TrackModel (fixture.track (trackPosition)).getSlots())
        if (slot.getScene() == scene)
            return slot.getClipId();

    return {};
}

// A clip "Riff" on the three tracks at tick 0 and in the first track's slot
// of scene 0: refcount 4. The setting up is not what a test takes back, so
// the undo history is empty afterwards.
struct SharedClip
{
    CommandFixture fixture;
    juce::String clipId = addedEventClip (fixture, "Riff");
    juce::String onBass = placeClip (fixture, 0, clipId, 0);
    juce::String onLead = placeClip (fixture, 1, clipId, 0);
    juce::String onBass2 = placeClip (fixture, 2, clipId, 0);

    SharedClip()
    {
        putClipInSlot (fixture, 0, 0, clipId);
        fixture.undoManager.clearUndoHistory();
        REQUIRE (fixture.isValid());
        REQUIRE (refcountOf (fixture, clipId) == 4);
    }

    commands::CommandResult rename (const juce::String& name, EditContext& context)
    {
        return commands::setClipName (fixture.project, clipId, name, context, fixture.undoManager);
    }
};

void collectIds (const juce::ValueTree& node, std::vector<juce::String>& found)
{
    if (node.hasProperty (ids::id))
        found.push_back (node[ids::id].toString());

    for (const auto& child : node)
        collectIds (child, found);
}

// The node as text with every id blanked: what a clone has in common with
// the clip it was made from.
juce::String withoutIds (const juce::ValueTree& node)
{
    auto copy = node.createCopy();

    const auto blank = [] (juce::ValueTree n, const auto& self) -> void {
        if (n.hasProperty (ids::id))
            n.setProperty (ids::id, "", nullptr);

        for (auto child : n)
            self (child, self);
    };

    blank (copy, blank);
    return copy.toXmlString();
}

} // namespace

TEST_CASE ("Under update all a change through one reference is a change for every reference, and no clone is made", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    EditContext context (Policy::updateAll);
    context.addPlacement (shared.onLead);

    REQUIRE (shared.rename ("Riff B", context).wasApplied());

    CHECK (shared.fixture.isValid());
    CHECK (clipNamesOf (shared.fixture) == "Riff B|");
    CHECK (refcountOf (shared.fixture, shared.clipId) == 4);
    CHECK (! context.hasClone());
}

TEST_CASE ("Under make unique the change lands on a clone that exactly the context's reference is redirected to", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    EditContext context (Policy::makeUnique);
    context.addPlacement (shared.onLead);

    REQUIRE (shared.rename ("Riff B", context).wasApplied());
    CHECK (fixture.isValid());

    // The clone stands behind the original, which is as it was.
    CHECK (clipNamesOf (fixture) == "Riff|Riff B|");
    REQUIRE (context.hasClone());
    CHECK (context.getOriginalId() == shared.clipId);
    CHECK (context.getCloneId() != shared.clipId);
    CHECK (clipsOf (fixture)[1].getId() == context.getCloneId());

    CHECK (clipIdOfPlacement (fixture, shared.onLead) == context.getCloneId());
    CHECK (clipIdOfPlacement (fixture, shared.onBass) == shared.clipId);
    CHECK (clipIdOfPlacement (fixture, shared.onBass2) == shared.clipId);
    CHECK (clipIdInSlot (fixture, 0, 0) == shared.clipId);
    CHECK (refcountOf (fixture, shared.clipId) == 3);
    CHECK (refcountOf (fixture, context.getCloneId()) == 1);
}

TEST_CASE ("Clone, redirection and change are one undo step", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    EditContext context (Policy::makeUnique);
    context.addPlacement (shared.onLead);
    context.addSlot (trackId (fixture, 0), 0);
    REQUIRE (shared.rename ("Riff B", context).wasApplied());
    const auto after = fixture.xml();
    REQUIRE (after != before);

    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
    CHECK (! fixture.undoManager.canUndo());

    REQUIRE (fixture.undoManager.redo());
    CHECK (fixture.xml() == after);
    CHECK (fixture.isValid());
}

TEST_CASE ("A clone is the clip again with a new id for the clip and for every node in it that has one", "[model][commands][RefcountGate]")
{
    CommandFixture fixture;

    // A step clip with two lanes, a cell with a CcValue and a skipped
    // column, put into the pool as a loaded file would hold it.
    const auto stepClip = juce::ValueTree::fromXml (
        R"(<Clip id="11111111111111111111111111111111" name="Drums" kind="step" colour="#ff102030">
             <Grid stepLength="120" stepCount="2" skipMask="01">
               <Lane id="22222222222222222222222222222222" name="Kick" fixedPitch="36">
                 <Cell on="1" gate="0.25"><CcValue cc="74" value="90"/></Cell><Cell/>
               </Lane>
               <Lane id="33333333333333333333333333333333">
                 <Cell pitch="48" on="1" velocity="90"/><Cell/>
               </Lane>
             </Grid>
           </Clip>)");
    fixture.project.getChildWithName (ids::ClipPool).appendChild (stepClip, nullptr);
    const juce::String clipId = "11111111111111111111111111111111";
    const auto first = placeClip (fixture, 0, clipId, 0);
    placeClip (fixture, 1, clipId, 0);
    REQUIRE (fixture.isValid());
    const auto originalBefore = clipNode (fixture, clipId).toXmlString();

    EditContext context (Policy::makeUnique);
    context.addPlacement (first);
    REQUIRE (commands::setClipColour (fixture.project, clipId, "#ffffffff", context, fixture.undoManager).wasApplied());

    // Valid, so no id is there twice.
    CHECK (fixture.isValid());
    REQUIRE (context.hasClone());

    const auto original = clipNode (fixture, clipId);
    auto clone = clipNode (fixture, context.getCloneId());
    REQUIRE (clone.isValid());
    CHECK (original.toXmlString() == originalBefore);

    std::vector<juce::String> originalIds;
    std::vector<juce::String> cloneIds;
    collectIds (original, originalIds);
    collectIds (clone, cloneIds);
    REQUIRE (originalIds.size() == 3);
    REQUIRE (cloneIds.size() == 3);

    std::set<juce::String> all (originalIds.begin(), originalIds.end());
    all.insert (cloneIds.begin(), cloneIds.end());
    CHECK (all.size() == 6);

    // But for the ids and the colour that was set, the same clip.
    CHECK (model::ClipModel (clone).getColour() == "#ffffffff");
    clone = clone.createCopy();
    clone.setProperty (ids::colour, "#ff102030", nullptr);
    CHECK (withoutIds (clone) == withoutIds (original));
}

TEST_CASE ("An event clip is cloned with its notes and events", "[model][commands][RefcountGate]")
{
    CommandFixture fixture;
    const auto eventClip = juce::ValueTree::fromXml (
        R"(<Clip id="11111111111111111111111111111111" name="Line" kind="event" loopLength="1920" quantise="1/8">
             <Events>
               <Note tick="0" length="240" pitch="60"/>
               <Event tick="0" kind="cc" a="74" b="10"/>
               <Note tick="480" length="240" pitch="62" velocity="90" enabled="0"/>
             </Events>
           </Clip>)");
    fixture.project.getChildWithName (ids::ClipPool).appendChild (eventClip, nullptr);
    const juce::String clipId = "11111111111111111111111111111111";
    const auto first = placeClip (fixture, 0, clipId, 0);
    putClipInSlot (fixture, 1, 3, clipId);
    REQUIRE (fixture.isValid());

    EditContext context (Policy::makeUnique);
    context.addPlacement (first);
    REQUIRE (commands::setClipName (fixture.project, clipId, "Line B", context, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (context.hasClone());

    const model::ClipModel clone (clipNode (fixture, context.getCloneId()));
    CHECK (clone.getKind() == model::ClipModel::Kind::event);
    CHECK (clone.getLoopLength() == 1920);
    CHECK (clone.getQuantise() == "1/8");
    REQUIRE (clone.getNotes().size() == 2);
    CHECK (clone.getNotes()[1].getPitch() == 62);
    CHECK (clone.getEvents().size() == 1);
}

TEST_CASE ("Several references in one context get one shared clone", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    EditContext context (Policy::makeUnique);
    context.addPlacement (shared.onBass);
    context.addPlacement (shared.onBass2);
    context.addSlot (trackId (fixture, 0), 0);

    REQUIRE (shared.rename ("Riff B", context).wasApplied());

    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "Riff|Riff B|");
    CHECK (clipIdOfPlacement (fixture, shared.onBass) == context.getCloneId());
    CHECK (clipIdOfPlacement (fixture, shared.onBass2) == context.getCloneId());
    CHECK (clipIdInSlot (fixture, 0, 0) == context.getCloneId());
    CHECK (clipIdOfPlacement (fixture, shared.onLead) == shared.clipId);
    CHECK (refcountOf (fixture, shared.clipId) == 1);
    CHECK (refcountOf (fixture, context.getCloneId()) == 3);
}

TEST_CASE ("A slot alone is made unique like a placement", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    EditContext context (Policy::makeUnique);
    context.addSlot (trackId (fixture, 0), 0);

    REQUIRE (shared.rename ("Riff B", context).wasApplied());

    CHECK (fixture.isValid());
    CHECK (clipIdInSlot (fixture, 0, 0) == context.getCloneId());
    CHECK (refcountOf (fixture, shared.clipId) == 3);
}

TEST_CASE ("A placement on an AutomationLane is made unique like any other", "[model][commands][RefcountGate]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Sweep");
    const auto lane = commands::addAutomationLane (fixture.project, trackId (fixture, 0), "Cutoff", fixture.undoManager);
    REQUIRE (lane.wasApplied());
    const auto first = commands::addLanePlacement (fixture.project, lane.createdId, clipId, 0, 960, fixture.undoManager);
    const auto second = commands::addLanePlacement (fixture.project, lane.createdId, clipId, 960, 960, fixture.undoManager);
    REQUIRE (first.wasApplied());
    REQUIRE (second.wasApplied());

    EditContext context (Policy::makeUnique);
    context.addPlacement (second.createdId);
    REQUIRE (commands::setClipName (fixture.project, clipId, "Sweep B", context, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    REQUIRE (context.hasClone());
    CHECK (clipIdOfPlacement (fixture, first.createdId) == clipId);
    CHECK (clipIdOfPlacement (fixture, second.createdId) == context.getCloneId());
}

TEST_CASE ("Every later command of the gesture changes the clone and makes no second one", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    const auto before = fixture.xml();

    fixture.undoManager.beginNewTransaction();
    EditContext context (Policy::makeUnique);
    context.addPlacement (shared.onLead);

    REQUIRE (shared.rename ("Riff B", context).wasApplied());
    const auto cloneId = context.getCloneId();

    // Named by the clip the gesture began with, and by the clone.
    REQUIRE (shared.rename ("Riff C", context).wasApplied());
    REQUIRE (commands::setClipColour (fixture.project, cloneId, "#ff000000", context, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "Riff|Riff C|");
    CHECK (context.getCloneId() == cloneId);
    CHECK (model::ClipModel (clipNode (fixture, cloneId)).getColour() == "#ff000000");
    CHECK (model::ClipModel (clipNode (fixture, shared.clipId)).getColour().isEmpty());
    CHECK (refcountOf (fixture, shared.clipId) == 3);

    // The whole gesture is one undo step.
    REQUIRE (fixture.undoManager.undo());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("A clip with one reference is simply changed, also under make unique", "[model][commands][RefcountGate]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Solo");
    const auto placementId = placeClip (fixture, 0, clipId, 0);

    EditContext context (Policy::makeUnique);
    context.addPlacement (placementId);
    REQUIRE (commands::setClipName (fixture.project, clipId, "Solo 2", context, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "Solo 2|");
    CHECK (! context.hasClone());
    CHECK (clipIdOfPlacement (fixture, placementId) == clipId);
}

TEST_CASE ("A context without a reference is pool-level editing: update all, whatever the policy", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    EditContext context (Policy::makeUnique);

    REQUIRE (shared.rename ("Riff B", context).wasApplied());

    CHECK (shared.fixture.isValid());
    CHECK (clipNamesOf (shared.fixture) == "Riff B|");
    CHECK (refcountOf (shared.fixture, shared.clipId) == 4);
    CHECK (! context.hasClone());
}

TEST_CASE ("A context that names every reference of the clip still gets a clone, and the original stays in the pool unused", "[model][commands][RefcountGate]")
{
    CommandFixture fixture;
    const auto clipId = addedEventClip (fixture, "Riff");
    const auto first = placeClip (fixture, 0, clipId, 0);
    const auto second = placeClip (fixture, 1, clipId, 0);

    EditContext context (Policy::makeUnique);
    context.addPlacement (first);
    context.addPlacement (second);
    REQUIRE (commands::setClipName (fixture.project, clipId, "Riff B", context, fixture.undoManager).wasApplied());

    CHECK (fixture.isValid());
    CHECK (clipNamesOf (fixture) == "Riff|Riff B|");
    CHECK (refcountOf (fixture, clipId) == 0);
    CHECK (refcountOf (fixture, context.getCloneId()) == 2);
}

TEST_CASE ("A command that changes nothing, or is refused for a reason of its own, makes no clone", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    const auto before = fixture.xml();
    EditContext context (Policy::makeUnique);
    context.addPlacement (shared.onLead);

    // The name it has.
    CHECK (shared.rename ("Riff", context).wasApplied());
    CHECK (commands::setClipColour (fixture.project, shared.clipId, "", context, fixture.undoManager).wasApplied());

    // No colour.
    CHECK (! commands::setClipColour (fixture.project, shared.clipId, "red", context, fixture.undoManager).wasApplied());

    CHECK (fixture.isUntouchedSince (before));
    CHECK (! context.hasClone());
}

TEST_CASE ("The gate refuses a context whose references are not references of the clip, under either policy", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    const auto otherClipId = addedEventClip (fixture, "Other");
    const auto onOther = placeClip (fixture, 0, otherClipId, 1920);
    putClipInSlot (fixture, 1, 0, otherClipId);
    fixture.undoManager.clearUndoHistory();
    const auto before = fixture.xml();

    for (const auto policy : { Policy::updateAll, Policy::makeUnique })
    {
        EditContext noSuchPlacement (policy);
        noSuchPlacement.addPlacement (shared.onLead);
        noSuchPlacement.addPlacement (unknownId);

        EditContext placementOfAnotherClip (policy);
        placementOfAnotherClip.addPlacement (onOther);

        EditContext noSuchTrack (policy);
        noSuchTrack.addSlot (unknownId, 0);

        EditContext emptySlot (policy);
        emptySlot.addSlot (trackId (fixture, 0), 1);

        EditContext slotWithAnotherClip (policy);
        slotWithAnotherClip.addPlacement (shared.onLead);
        slotWithAnotherClip.addSlot (trackId (fixture, 1), 0);

        // A Track's id names no Placement.
        EditContext notAPlacement (policy);
        notAPlacement.addPlacement (trackId (fixture, 0));

        for (auto* context : { &noSuchPlacement, &placementOfAnotherClip, &noSuchTrack, &emptySlot, &slotWithAnotherClip, &notAPlacement })
        {
            const auto result = shared.rename ("Riff B", *context);

            CHECK (! result.wasApplied());
            CHECK (result.refusal.isNotEmpty());
            CHECK (! context->hasClone());
        }
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("The gate refuses a clip that is not there", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    const auto before = fixture.xml();
    EditContext context (Policy::makeUnique);

    for (const auto& id : { juce::String (unknownId), juce::String(), trackId (fixture, 0), shared.onLead })
    {
        const auto admission = RefcountGate::admit (fixture.project, id, context, fixture.undoManager);

        CHECK (! admission.wasAdmitted());
        CHECK (! admission.clip.isValid());
        CHECK (! admission.needsClone);
        CHECK (! commands::setClipName (fixture.project, id, "x", context, fixture.undoManager).wasApplied());
    }

    CHECK (fixture.isUntouchedSince (before));
}

TEST_CASE ("A gesture that has cloned changes no third clip", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    const auto otherClipId = addedEventClip (fixture, "Other");
    EditContext context (Policy::makeUnique);
    context.addPlacement (shared.onLead);
    REQUIRE (shared.rename ("Riff B", context).wasApplied());
    const auto before = fixture.xml();

    const auto result = commands::setClipName (fixture.project, otherClipId, "x", context, fixture.undoManager);

    CHECK (! result.wasApplied());
    CHECK (result.refusal.isNotEmpty());
    CHECK (fixture.xml() == before);
}

TEST_CASE ("Admitting changes nothing and says whether a clone is due; opening hands out the clip to change", "[model][commands][RefcountGate]")
{
    SharedClip shared;
    auto& fixture = shared.fixture;
    const auto before = fixture.xml();

    SECTION ("no clone due")
    {
        EditContext context (Policy::updateAll);
        context.addPlacement (shared.onLead);

        const auto admission = RefcountGate::admit (fixture.project, shared.clipId, context, fixture.undoManager);
        REQUIRE (admission.wasAdmitted());
        CHECK (! admission.needsClone);
        CHECK (admission.clip == clipNode (fixture, shared.clipId));

        CHECK (RefcountGate::open (fixture.project, admission, context, fixture.undoManager) == admission.clip);
        CHECK (fixture.isUntouchedSince (before));
    }

    SECTION ("a clone due")
    {
        EditContext context (Policy::makeUnique);
        context.addPlacement (shared.onLead);

        const auto admission = RefcountGate::admit (fixture.project, shared.clipId, context, fixture.undoManager);
        REQUIRE (admission.wasAdmitted());
        CHECK (admission.needsClone);
        CHECK (admission.clip == clipNode (fixture, shared.clipId));
        CHECK (fixture.isUntouchedSince (before));

        const auto opened = RefcountGate::open (fixture.project, admission, context, fixture.undoManager);
        CHECK (opened == clipNode (fixture, context.getCloneId()));
        CHECK (opened != admission.clip);
        CHECK (fixture.isValid());

        // From now on the clone is what is admitted, and no clone is due.
        const auto again = RefcountGate::admit (fixture.project, shared.clipId, context, fixture.undoManager);
        REQUIRE (again.wasAdmitted());
        CHECK (! again.needsClone);
        CHECK (again.clip == opened);
    }
}

TEST_CASE ("A context that has cloned is known to belong to an earlier gesture once that gesture is over", "[model][commands][RefcountGate]")
{
    // What the gate asserts on. The command itself is not called with such
    // a context here: a failed assertion fails the test, as it should.
    SharedClip shared;
    auto& fixture = shared.fixture;

    fixture.undoManager.beginNewTransaction();
    EditContext context (Policy::makeUnique);
    context.addPlacement (shared.onLead);
    REQUIRE (shared.rename ("Riff B", context).wasApplied());
    CHECK (! context.belongsToAnEarlierGesture (fixture.undoManager));

    SECTION ("the next gesture has begun")
    {
        fixture.undoManager.beginNewTransaction();
        CHECK (context.belongsToAnEarlierGesture (fixture.undoManager));
    }

    SECTION ("the gesture was undone, and its clone with it")
    {
        REQUIRE (fixture.undoManager.undo());
        CHECK (context.belongsToAnEarlierGesture (fixture.undoManager));
        CHECK (! clipNode (fixture, context.getCloneId()).isValid());
    }
}
