// Planted bug: heap use-after-free. This canary passes only when AddressSanitizer
// reports it, so a build that has silently lost -fsanitize=address fails the gate.

#include <cstdio>

int main()
{
    auto* value = new int (42);
    delete value;

    // volatile keeps the optimiser from proving the read away.
    volatile int readBack = *value;
    std::printf ("canary read %d without a sanitizer report\n", readBack);
    return 0;
}
