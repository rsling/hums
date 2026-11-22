#include "model/ColourText.h"

namespace model
{

bool ColourText::isValid (const juce::String& text)
{
    if (text.isEmpty())
        return true;

    return text.length() == 9 && text.startsWithChar ('#') && text.substring (1).containsOnly ("0123456789abcdefABCDEF");
}

} // namespace model
