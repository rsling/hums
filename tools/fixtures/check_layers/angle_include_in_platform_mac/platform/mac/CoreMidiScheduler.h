// Fixture for tools/check_layers.py: this tree must fail the check.
// platform/mac/ may include OS headers with <...>, but a header of ours is not
// one: in angle brackets it would get past the layer table, here into engine/.
#pragma once

#include <CoreMIDI/CoreMIDI.h>
#include <engine/Transport.h>
