// Planted bug: a leaked object of a class with JUCE's leak detector. JUCE
// only prints "*** Leaked objects detected: ..." when the process ends, and
// the exit code stays 0. The test is registered as one that must fail
// (tests/CMakeLists.txt, hums_fail_on_juce_reports): it passes only while
// that printed line still fails a test.

#include <juce_core/juce_core.h>

namespace
{
struct Leaked
{
    int value = 0;
    JUCE_LEAK_DETECTOR (Leaked)
};
} // namespace

int main()
{
    // volatile keeps the compiler from removing the allocation.
    Leaked* volatile leaked = new Leaked();
    leaked->value = 1;
    return 0;
}
