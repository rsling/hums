// Fixture for tools/check_layers.py: this tree must fail the check.
// An include without a literal file name cannot be checked.
#pragma once

#define HUMS_HEADER "model/Ids.h"
#include HUMS_HEADER
