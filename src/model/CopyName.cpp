#include "model/CopyName.h"

#include "model/ClipPoolModel.h"
#include "model/Ids.h"

#include <set>

namespace model
{

namespace
{

    const juce::String suffixStart { "(copy " };

    juce::String withNumber (const juce::String& base, int number)
    {
        const auto suffix = suffixStart + juce::String (number) + ")";

        return base.isEmpty() ? suffix : base + " " + suffix;
    }

    // True for the digits of a whole number from 1 on, written as
    // withNumber writes it: no sign, no leading zero.
    bool isACopyNumber (const juce::String& text)
    {
        return text.isNotEmpty() && text.containsOnly ("0123456789") && ! text.startsWithChar ('0');
    }

} // namespace

juce::String CopyName::withoutSuffix (const juce::String& name)
{
    if (! name.endsWithChar (')'))
        return name;

    const auto start = name.lastIndexOf (suffixStart);

    if (start < 0)
        return name;

    const auto number = name.substring (start + suffixStart.length(), name.length() - 1);

    if (! isACopyNumber (number))
        return name;

    // The suffix alone is the copy of a clip without a name.
    if (start == 0)
        return {};

    if (name[start - 1] != ' ')
        return name;

    return name.substring (0, start - 1);
}

juce::String CopyName::forACopyOf (const juce::String& originalName, const juce::ValueTree& clipPool)
{
    jassert (clipPool.hasType (ids::ClipPool));

    std::set<juce::String> namesInUse;

    for (const auto& clip : ClipPoolModel (clipPool).getClips())
        namesInUse.insert (clip.getName());

    const auto base = withoutSuffix (originalName);

    // Ends: the pool has fewer clips than there are numbers.
    for (int number = 1;; ++number)
        if (const auto candidate = withNumber (base, number); namesInUse.count (candidate) == 0)
            return candidate;
}

} // namespace model
