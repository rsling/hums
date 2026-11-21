// model::EventModel: one non-note MIDI event of an event clip.
// docs/SCHEMA.md -> Event clip: Events.

#include <catch2/catch_test_macros.hpp>

#include "model/EventModel.h"

namespace
{

model::EventModel::Kind kindOf (const char* xml)
{
    return model::EventModel (juce::ValueTree::fromXml (xml)).getKind();
}

} // namespace

TEST_CASE ("An Event gives back what is stored", "[model][EventModel]")
{
    const model::EventModel event (juce::ValueTree::fromXml (
        R"(<Event tick="240" kind="cc" a="74" b="90" enabled="0"/>)"));

    CHECK (event.getTick() == 240);
    CHECK (event.getKind() == model::EventModel::Kind::cc);
    CHECK (event.getA() == 74);
    CHECK (event.getB() == 90);
    CHECK (event.getData() == "");
    CHECK_FALSE (event.isEnabled());
}

TEST_CASE ("An Event without a, b and data is enabled and has zeros", "[model][EventModel]")
{
    const model::EventModel event (juce::ValueTree::fromXml (R"(<Event tick="0" kind="programChange"/>)"));

    CHECK (event.getA() == 0);
    CHECK (event.getB() == 0);
    CHECK (event.getData() == "");
    CHECK (event.isEnabled());
}

TEST_CASE ("Every kind of Event the schema lists is known", "[model][EventModel]")
{
    using Kind = model::EventModel::Kind;

    CHECK (kindOf (R"(<Event tick="0" kind="cc"/>)") == Kind::cc);
    CHECK (kindOf (R"(<Event tick="0" kind="pitchBend"/>)") == Kind::pitchBend);
    CHECK (kindOf (R"(<Event tick="0" kind="programChange"/>)") == Kind::programChange);
    CHECK (kindOf (R"(<Event tick="0" kind="channelPressure"/>)") == Kind::channelPressure);
    CHECK (kindOf (R"(<Event tick="0" kind="polyPressure"/>)") == Kind::polyPressure);
    CHECK (kindOf (R"(<Event tick="0" kind="sysex"/>)") == Kind::sysex);
}

TEST_CASE ("A pitch bend holds a 14-bit value in a", "[model][EventModel]")
{
    const model::EventModel event (juce::ValueTree::fromXml (R"(<Event tick="0" kind="pitchBend" a="16383"/>)"));

    CHECK (event.getA() == 16383);
}

TEST_CASE ("A SysEx Event holds its payload as hex text", "[model][EventModel]")
{
    const model::EventModel event (juce::ValueTree::fromXml (R"(<Event tick="0" kind="sysex" data="7E7F0601"/>)"));

    CHECK (event.getKind() == model::EventModel::Kind::sysex);
    CHECK (event.getData() == "7E7F0601");
}
