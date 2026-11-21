// Fixture for tools/check_layers.py: this tree must fail the check.
// An angle-bracket include that is neither std nor JUCE counts as an OS header.
#include <pthread.h>
