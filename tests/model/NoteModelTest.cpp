// model::NoteModel: one note of an event clip. docs/SCHEMA.md -> Event clip: Events.

#include <catch2/catch_test_macros.hpp>

#include "model/NoteModel.h"

TEST_CASE ("A Note gives back what is stored", "[model][NoteModel]")
{
    const model::NoteModel note (juce::ValueTree::fromXml (
        R"(<Note tick="480" length="240" pitch="64" velocity="90" offVelocity="0" enabled="0"/>)"));

    CHECK (note.getTick() == 480);
    CHECK (note.getLength() == 240);
    CHECK (note.getPitch() == 64);
    CHECK (note.getVelocity() == 90);
    CHECK (note.getOffVelocity() == 0);
    CHECK_FALSE (note.isEnabled());
}

TEST_CASE ("A Note without velocities is enabled, 100 on and 64 off", "[model][NoteModel]")
{
    const model::NoteModel note (juce::ValueTree::fromXml (R"(<Note tick="0" length="960" pitch="60"/>)"));

    CHECK (note.getVelocity() == 100);
    CHECK (note.getOffVelocity() == 64);
    CHECK (note.isEnabled());
}

TEST_CASE ("A Note's tick and length are 64-bit", "[model][NoteModel]")
{
    const model::NoteModel note (juce::ValueTree::fromXml (
        R"(<Note tick="5000000000" length="6000000000" pitch="60"/>)"));

    CHECK (note.getTick() == 5000000000LL);
    CHECK (note.getLength() == 6000000000LL);
}
