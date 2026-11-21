// model::TempoEventModel: a tempo on the Timeline. docs/SCHEMA.md -> Timeline.

#include <catch2/catch_test_macros.hpp>

#include "model/TempoEventModel.h"

TEST_CASE ("A TempoEvent gives back its tick and the tempo as entered", "[model][TempoEventModel]")
{
    const model::TempoEventModel tempo (juce::ValueTree::fromXml (R"(<TempoEvent tick="7680" bpm="118.5"/>)"));

    CHECK (tempo.getTick() == 7680);
    CHECK (juce::exactlyEqual (tempo.getBpm(), 118.5));
}

TEST_CASE ("A TempoEvent's tick is 64 bits wide", "[model][TempoEventModel]")
{
    const model::TempoEventModel tempo (juce::ValueTree::fromXml (R"(<TempoEvent tick="5000000000" bpm="120"/>)"));

    CHECK (tempo.getTick() == 5'000'000'000);
    CHECK (juce::exactlyEqual (tempo.getBpm(), 120.0));
}
