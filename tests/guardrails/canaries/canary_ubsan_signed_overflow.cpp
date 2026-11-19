// Planted bug: signed integer overflow. This canary passes only when
// UndefinedBehaviorSanitizer reports it, so a build that has silently lost
// -fsanitize=undefined fails the gate.

#include <climits>
#include <cstdio>

int main()
{
    // volatile keeps the compiler from folding the overflow at compile time.
    volatile int large = INT_MAX;
    int overflowed = large + 1;
    std::printf ("canary computed %d without a sanitizer report\n", overflowed);
    return 0;
}
