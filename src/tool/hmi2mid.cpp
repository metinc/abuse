//
// Abuse HMI to Standard MIDI build tool
//
// Copyright (c) 2011 Jochen Schleu <jjs@jjs.at>
// Copyright (c) 2024-2025 Andrej Pancik
// Licensed under the WTFPL. See COPYING.WTFPL for details.
//

#include <cstdio>

#include "sdlport/hmi.h"

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "Usage: abuse-hmi2mid <input.hmi> <output.mid>\n");
        return 1;
    }

    if (!convert_hmi_to_midi_file(argv[1], argv[2]))
    {
        fprintf(stderr, "abuse-hmi2mid: could not convert %s to %s\n", argv[1], argv[2]);
        return 1;
    }
    return 0;
}
