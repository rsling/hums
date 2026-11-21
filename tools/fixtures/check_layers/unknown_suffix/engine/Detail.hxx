// Fixture for tools/check_layers.py: this tree must fail the check.
// The check does not read a .hxx file, so this one could include anything;
// a file it does not read is refused for that alone.
#pragma once

#include "model/Ids.h"
#include <CoreMIDI/CoreMIDI.h>
