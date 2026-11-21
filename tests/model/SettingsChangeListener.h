#pragma once

#include "model/Settings.h"

#include <vector>

// A listener for the Settings tests: notes every property change it hears of,
// in order, and whether any of them showed it an enabled transport loop whose
// start is not before its end (docs/SCHEMA.md -> UiState).
class SettingsChangeListener : public juce::ValueTree::Listener
{
public:
    void valueTreePropertyChanged (juce::ValueTree& changedNode, const juce::Identifier& property) override
    {
        changedProperties.push_back (property);

        const model::Settings settings (changedNode);
        const auto loop = settings.getTransportLoop();

        if (loop.enabled && loop.start >= loop.end)
            sawInvalidLoop = true;
    }

    std::vector<juce::Identifier> changedProperties;
    bool sawInvalidLoop = false;
};
