// Fixture for tools/check_layers.py: this tree must fail the check.
// With ".." a path leaves the include directory and can come back into the
// source root under a name the check cannot follow. No file of that name is
// in this tree: the ".." alone is refused.
#include <../src/model/Ids.h>
