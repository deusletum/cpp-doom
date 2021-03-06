//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Parses "Frame" sections in dehacked files
//

#include <cstdio>
#include <cstdlib>

#include "info.hpp"
#include "deh_defs.hpp"
#include "deh_io.hpp"
#include "deh_main.hpp"
#include "deh_mapping.hpp"

DEH_BEGIN_MAPPING(state_mapping, state_t)
DEH_MAPPING("Sprite number", sprite)
DEH_MAPPING("Sprite subnumber", frame)
DEH_MAPPING("Duration", tics)
DEH_MAPPING("Next frame", nextstate)
DEH_MAPPING("Unknown 1", misc1)
DEH_MAPPING("Unknown 2", misc2)
DEH_UNSUPPORTED_MAPPING("Codep frame")
DEH_END_MAPPING

static void *DEH_FrameStart(deh_context_t *context, char *line)
{
    int      frame_number = 0;
    state_t *state = nullptr;

    if (sscanf(line, "Frame %i", &frame_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return nullptr;
    }

    if (frame_number < 0 || frame_number >= NUMSTATES)
    {
        DEH_Warning(context, "Invalid frame number: %i", frame_number);
        return nullptr;
    }

    if (frame_number >= DEH_VANILLA_NUMSTATES)
    {
        DEH_Warning(context, "Attempt to modify frame %i: this will cause "
                             "problems in Vanilla dehacked.",
            frame_number);
    }

    state = &states[frame_number];

    return state;
}

// Simulate a frame overflow: Doom has 967 frames in the states[] array, but
// DOS dehacked internally only allocates memory for 966.  As a result,
// attempts to set frame 966 (the last frame) will overflow the dehacked
// array and overwrite the weaponinfo[] array instead.
//
// This is noticable in Batman Doom where it is impossible to switch weapons
// away from the fist once selected.

static void DEH_FrameParseLine(deh_context_t *context, char *line, void *tag)
{
    state_t *state = nullptr;
    char *   variable_name = nullptr, *value = nullptr;
    int      ivalue = 0;

    if (tag == nullptr)
        return;

    state = reinterpret_cast<state_t *>(tag);

    // Parse the assignment

    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse

        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    // all values are integers

    ivalue = std::atoi(value);


    DEH_SetMapping(context, &state_mapping, state, variable_name, ivalue);
}

static void DEH_FrameSHA1Sum(sha1_context_t *context)
{
    for (auto & state : states)
    {
        DEH_StructSHA1Sum(context, &state_mapping, &state);
    }
}

deh_section_t deh_section_frame = {
    "Frame",
    nullptr,
    DEH_FrameStart,
    DEH_FrameParseLine,
    nullptr,
    DEH_FrameSHA1Sum,
};
