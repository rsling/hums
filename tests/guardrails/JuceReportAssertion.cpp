// Planted bug: a failed jassert. Outside a debugger JUCE only prints
// "JUCE Assertion failure in <file>:<line>" and carries on, and this program
// exits 0. The test is registered as one that must fail (tests/CMakeLists.txt,
// hums_fail_on_juce_reports): it passes only while that printed line still
// fails a test.

#include <juce_core/juce_core.h>

int main()
{
    jassertfalse;
    return 0;
}
