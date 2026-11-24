// Fixture for tools/check_layers.py: this tree must fail the check.
// A raw string ends at its own closing bracket: the quote and the /* in the
// first one are text, and no comment begins there that would hold the include.
#pragma once

inline const char* const opening = R"(" /*)";
#include "model/Ids.h"
inline const char* const closing = "*/";
