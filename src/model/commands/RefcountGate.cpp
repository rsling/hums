#include "model/commands/RefcountGate.h"

#include "model/CopyName.h"
#include "model/Ids.h"
#include "model/PropertyReader.h"
#include "model/TracksModel.h"
#include "model/commands/CloneMark.h"
#include "model/commands/NodeLookup.h"

#include <vector>

namespace model::commands
{

namespace
{

    RefcountGate::Admission refused (const juce::String& reason)
    {
        RefcountGate::Admission admission;
        admission.refusal = reason;
        return admission;
    }

    juce::ValueTree clipOf (const juce::ValueTree& project, const juce::String& clipId)
    {
        return NodeLookup::childWithId (project.getChildWithName (ids::ClipPool), ids::Clip, clipId);
    }

    // The Slot node of that track in that row; invalid when the track is
    // not there or the slot is empty.
    juce::ValueTree slotOf (const juce::ValueTree& project, const EditContext::SlotReference& reference)
    {
        const auto track = NodeLookup::childWithId (project.getChildWithName (ids::Tracks), ids::Track, reference.trackId);

        return NodeLookup::childWithNumber (track.getChildWithName (ids::Slots), ids::Slot, ids::scene, reference.scene);
    }

    // The Placement and Slot nodes the context names, in its order. One
    // that is not there comes as an invalid node.
    std::vector<juce::ValueTree> referencesOf (const juce::ValueTree& project, const EditContext& editContext)
    {
        std::vector<juce::ValueTree> references;

        for (const auto& placementId : editContext.getPlacementIds())
            references.push_back (NodeLookup::placementWithId (project, placementId));

        for (const auto& slot : editContext.getSlots())
            references.push_back (slotOf (project, slot));

        return references;
    }

    // Why the gesture cannot be editing this clip through the references
    // of its context; empty when it can.
    juce::String whyNotThrough (const juce::ValueTree& project, const EditContext& editContext, const juce::String& clipId)
    {
        for (const auto& placementId : editContext.getPlacementIds())
        {
            const auto placement = NodeLookup::placementWithId (project, placementId);

            if (! placement.isValid())
                return "The edit context names a Placement with the id \"" + placementId + "\", and there is none.";

            if (PropertyReader::getRequiredString (placement, ids::clipId) != clipId)
                return "The edit context names the Placement \"" + placementId + "\", which references another clip than the one to change.";
        }

        for (const auto& reference : editContext.getSlots())
        {
            const auto where = "the slot of track \"" + reference.trackId + "\" in scene " + juce::String (reference.scene);
            const auto slot = slotOf (project, reference);

            if (! slot.isValid())
                return "The edit context names " + where + ", and there is no such track or the slot is empty.";

            if (PropertyReader::getRequiredString (slot, ids::clipId) != clipId)
                return "The edit context names " + where + ", which holds another clip than the one to change.";
        }

        return {};
    }

    void giveNewIds (juce::ValueTree node)
    {
        if (node.hasProperty (ids::id))
            node.setProperty (ids::id, juce::Uuid().toString(), nullptr);

        for (auto child : node)
            giveNewIds (child);
    }

} // namespace

RefcountGate::Admission RefcountGate::admit (const juce::ValueTree& project, const juce::String& clipId, const EditContext& editContext, const juce::UndoManager& undoManager)
{
    jassert (project.hasType (ids::Project));

    if (editContext.belongsToAnEarlierGesture (undoManager))
    {
        // A view has kept its context beyond its gesture.
        jassertfalse;
        return refused ("The edit context belongs to an earlier gesture.");
    }

    Admission admission;

    if (editContext.hasClone())
    {
        if (clipId != editContext.getOriginalId() && clipId != editContext.getCloneId())
            return refused ("The edit context is editing another clip than \"" + clipId + "\": one gesture changes one clip.");

        admission.clip = clipOf (project, editContext.getCloneId());

        if (! admission.clip.isValid())
            return refused ("The clip that this gesture made unique, \"" + editContext.getCloneId() + "\", is no longer in the pool.");

        return admission;
    }

    admission.clip = clipOf (project, clipId);

    if (! admission.clip.isValid())
        return refused ("There is no Clip with the id \"" + clipId + "\".");

    // Held under either policy, so that a gesture is right or wrong
    // whatever the toggle says.
    const auto whyNot = whyNotThrough (project, editContext, clipId);

    if (whyNot.isNotEmpty())
        return refused (whyNot);

    const auto refcount = TracksModel (project.getChildWithName (ids::Tracks)).getRefcount (clipId);

    admission.needsClone = refcount > 1
                        && editContext.hasReferences()
                        && editContext.getPolicy() == Settings::SharedEditPolicy::makeUnique;

    return admission;
}

juce::ValueTree RefcountGate::open (juce::ValueTree project, const Admission& admission, EditContext& editContext, juce::UndoManager& undoManager)
{
    jassert (project.hasType (ids::Project));
    jassert (admission.wasAdmitted() && admission.clip.isValid());

    if (! admission.needsClone)
    {
        editContext.rememberUse (undoManager);
        return admission.clip;
    }

    const auto& original = admission.clip;
    const auto references = referencesOf (project, editContext);

    // Built apart and added whole, so that undo knows one action.
    auto clone = original.createCopy();
    giveNewIds (clone);
    const auto cloneId = PropertyReader::getRequiredString (clone, ids::id);

    // Told apart from its original in the pool [D20].
    auto clipPool = original.getParent();
    clone.setProperty (ids::name, CopyName::forACopyOf (PropertyReader::getString (original, ids::name, ""), clipPool), nullptr);

    clipPool.addChild (clone, clipPool.indexOf (original) + 1, &undoManager);

    for (auto reference : references)
        reference.setProperty (ids::clipId, cloneId, &undoManager);

    // By this the context knows the gesture's transaction from every later
    // one.
    CloneMark::leaveIn (undoManager, cloneId);

    editContext.rememberClone (PropertyReader::getRequiredString (original, ids::id), cloneId);
    editContext.rememberUse (undoManager);

    return clone;
}

} // namespace model::commands
