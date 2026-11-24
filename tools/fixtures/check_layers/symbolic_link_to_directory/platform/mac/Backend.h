// Fixture for tools/check_layers.py: this tree must fail the check.
// hidden beside this file is a symbolic link to model/. The check does not go
// into it, so the include below would pass for an OS header. The link is what
// is reported.
#pragma once

#include <platform/mac/hidden/Ids.h>
