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
// Parses "Ammo" sections in dehacked files
//

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "doomdef.hpp"
#include "deh_defs.hpp"
#include "deh_io.hpp"
#include "deh_main.hpp"
#include "p_local.hpp"

static void *DEH_AmmoStart(deh_context_t *context, char *line)
{
    int ammo_number = 0;

    if (sscanf(line, "Ammo %i", &ammo_number) != 1)
    {
        DEH_Warning(context, "Parse error on section start");
        return nullptr;
    }

    if (ammo_number < 0 || ammo_number >= NUMAMMO)
    {
        DEH_Warning(context, "Invalid ammo number: %i", ammo_number);
        return nullptr;
    }

    return &g_p_local_globals->maxammo[ammo_number];
}

static void DEH_AmmoParseLine(deh_context_t *context, char *line, void *tag)
{
    if (tag == nullptr)
        return;

    int ammo_number = static_cast<int>(reinterpret_cast<int *>(tag) - g_p_local_globals->maxammo);

    // Parse the assignment
    char *variable_name = nullptr;
    char *value = nullptr;
    if (!DEH_ParseAssignment(line, &variable_name, &value))
    {
        // Failed to parse

        DEH_Warning(context, "Failed to parse assignment");
        return;
    }

    int ivalue = std::atoi(value);

    // maxammo

    if (!strcasecmp(variable_name, "Per ammo"))
        g_p_local_globals->clipammo[ammo_number] = ivalue;
    else if (!strcasecmp(variable_name, "Max ammo"))
        g_p_local_globals->maxammo[ammo_number] = ivalue;
    else
    {
        DEH_Warning(context, "Field named '%s' not found", variable_name);
    }
}

static void DEH_AmmoSHA1Hash(sha1_context_t *context)
{
    for (int i = 0; i < NUMAMMO; ++i)
    {
        SHA1_UpdateInt32(context, static_cast<unsigned int>(g_p_local_globals->clipammo[i]));
        SHA1_UpdateInt32(context, static_cast<unsigned int>(g_p_local_globals->maxammo[i]));
    }
}

deh_section_t deh_section_ammo = {
    "Ammo",
    nullptr,
    DEH_AmmoStart,
    DEH_AmmoParseLine,
    nullptr,
    DEH_AmmoSHA1Hash,
};
