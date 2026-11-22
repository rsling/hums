// model/CopyName: the name a clip gets when it is made as a copy of another
// [D20].

#include "model/CopyName.h"

#include "model/Ids.h"

#include <catch2/catch_test_macros.hpp>
#include <juce_data_structures/juce_data_structures.h>

#include <initializer_list>

namespace
{

namespace ids = model::ids;

juce::ValueTree poolWithClipsNamed (std::initializer_list<const char*> names)
{
    juce::ValueTree clipPool (ids::ClipPool);

    for (const auto* name : names)
    {
        juce::ValueTree clip (ids::Clip);
        clip.setProperty (ids::name, juce::String (name), nullptr);
        clipPool.appendChild (clip, nullptr);
    }

    return clipPool;
}

std::string copyNameOf (const char* originalName, const juce::ValueTree& clipPool)
{
    return model::CopyName::forACopyOf (originalName, clipPool).toStdString();
}

std::string withoutSuffix (const char* name)
{
    return model::CopyName::withoutSuffix (name).toStdString();
}

} // namespace

TEST_CASE ("CopyName: the first copy of a clip is its name with (copy 1)", "[model][copyname]")
{
    CHECK (copyNameOf ("Bass", poolWithClipsNamed ({ "Bass" })) == "Bass (copy 1)");
}

TEST_CASE ("CopyName: the number is the smallest that no clip of the pool has", "[model][copyname]")
{
    CHECK (copyNameOf ("Bass", poolWithClipsNamed ({ "Bass", "Bass (copy 1)" })) == "Bass (copy 2)");
    CHECK (copyNameOf ("Bass", poolWithClipsNamed ({ "Bass", "Bass (copy 1)", "Bass (copy 3)" })) == "Bass (copy 2)");
    CHECK (copyNameOf ("Bass", poolWithClipsNamed ({ "Bass", "Bass (copy 2)" })) == "Bass (copy 1)");
}

TEST_CASE ("CopyName: a copy of a copy replaces the suffix and does not stack it", "[model][copyname]")
{
    CHECK (copyNameOf ("Bass (copy 1)", poolWithClipsNamed ({ "Bass", "Bass (copy 1)", "Bass (copy 2)" })) == "Bass (copy 3)");
    CHECK (copyNameOf ("Bass (copy 7)", poolWithClipsNamed ({ "Bass (copy 7)" })) == "Bass (copy 1)");
}

TEST_CASE ("CopyName: a clip without a name gives the suffix alone", "[model][copyname]")
{
    CHECK (copyNameOf ("", poolWithClipsNamed ({ "" })) == "(copy 1)");
    CHECK (copyNameOf ("(copy 1)", poolWithClipsNamed ({ "", "(copy 1)" })) == "(copy 2)");
}

TEST_CASE ("CopyName: the names of other clips do not count", "[model][copyname]")
{
    CHECK (copyNameOf ("Bass", poolWithClipsNamed ({ "Bass", "Lead (copy 1)", "Bass (copy 1) B" })) == "Bass (copy 1)");
}

TEST_CASE ("CopyName: only a suffix as it is written is taken off", "[model][copyname]")
{
    CHECK (withoutSuffix ("Bass (copy 12)") == "Bass");
    CHECK (withoutSuffix ("(copy 4)") == "");
    CHECK (withoutSuffix ("Bass (copy 1) (copy 2)") == "Bass (copy 1)");

    CHECK (withoutSuffix ("Bass") == "Bass");
    CHECK (withoutSuffix ("Bass (copy)") == "Bass (copy)");
    CHECK (withoutSuffix ("Bass (copy 0)") == "Bass (copy 0)");
    CHECK (withoutSuffix ("Bass (copy 01)") == "Bass (copy 01)");
    CHECK (withoutSuffix ("Bass (copy -1)") == "Bass (copy -1)");
    CHECK (withoutSuffix ("Bass (copy 1) ") == "Bass (copy 1) ");
    CHECK (withoutSuffix ("Bass(copy 1)") == "Bass(copy 1)");
    CHECK (withoutSuffix ("Bass (Copy 1)") == "Bass (Copy 1)");
}
