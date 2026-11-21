#include "model/ClipPoolModel.h"

#include "model/Ids.h"

#include <utility>

namespace model
{

ClipPoolModel::ClipPoolModel (juce::ValueTree clipPoolNode)
    : node (std::move (clipPoolNode))
{
    jassert (node.hasType (ids::ClipPool));
}

std::vector<ClipModel> ClipPoolModel::getClips() const
{
    std::vector<ClipModel> clips;

    for (const auto& child : node)
        if (child.hasType (ids::Clip))
            clips.emplace_back (child);

    return clips;
}

std::optional<ClipModel> ClipPoolModel::findClip (const juce::String& clipId) const
{
    for (const auto& clip : getClips())
        if (clip.getId() == clipId)
            return clip;

    return std::nullopt;
}

} // namespace model
