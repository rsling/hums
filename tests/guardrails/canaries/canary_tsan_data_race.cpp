// Planted bug: two threads write one int without synchronisation. This canary
// passes only when ThreadSanitizer reports the race, so a build that has
// silently lost -fsanitize=thread fails the gate.

#include <cstdio>
#include <thread>

namespace
{
int sharedCounter = 0;

void increment()
{
    for (int i = 0; i < 1000; ++i)
        ++sharedCounter;
}
} // namespace

int main()
{
    std::thread first (increment);
    std::thread second (increment);
    first.join();
    second.join();

    std::printf ("canary counted %d without a sanitizer report\n", sharedCounter);
    return 0;
}
