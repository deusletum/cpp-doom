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
// DESCRIPTION:
//    PC speaker interface.
//

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "pcsound.hpp"
#include "pcsound_internal.hpp"


#ifdef HAVE_DEV_ISA_SPKRIO_H
#define HAVE_BSD_SPEAKER
#endif
#ifdef HAVE_DEV_SPEAKER_SPEAKER_H
#define HAVE_BSD_SPEAKER
#endif

#ifdef _WIN32
extern pcsound_driver_t pcsound_win32_driver;
#endif

#ifdef HAVE_BSD_SPEAKER
extern pcsound_driver_t pcsound_bsd_driver;
#endif

#ifdef HAVE_LINUX_KD_H
extern pcsound_driver_t pcsound_linux_driver;
#endif

extern pcsound_driver_t pcsound_sdl_driver;

static pcsound_driver_t *drivers[] = 
{
#ifdef HAVE_LINUX_KD_H
    &pcsound_linux_driver,
#endif
#ifdef HAVE_BSD_SPEAKER
    &pcsound_bsd_driver,
#endif
#ifdef _WIN32
    &pcsound_win32_driver,
#endif
    &pcsound_sdl_driver,
    nullptr,
};

static pcsound_driver_t *pcsound_driver = nullptr;

int pcsound_sample_rate;

void PCSound_SetSampleRate(int rate)
{
    pcsound_sample_rate = rate;
}

int PCSound_Init(pcsound_callback_func callback_func)
{
    if (pcsound_driver != nullptr)
    {
        return 1;
    }

    // Check if the environment variable is set

    char *driver_name = getenv("PCSOUND_DRIVER");

    if (driver_name != nullptr)
    {
        for (int i=0; drivers[i] != nullptr; ++i)
        {
            if (!strcmp(drivers[i]->name, driver_name))
            {
                // Found the driver!

                if (drivers[i]->init_func(callback_func))
                {
                    pcsound_driver = drivers[i];
                }
                else
                {
                    printf("Failed to initialize PC sound driver: %s\n",
                           drivers[i]->name);
                    break;
                }
            }
        }
    }
    else
    {
        // Try all drivers until we find a working one

        for (int i=0; drivers[i] != nullptr; ++i)
        {
            if (drivers[i]->init_func(callback_func)) 
            {
                pcsound_driver = drivers[i];
                break;
            }
        }
    }
    
    if (pcsound_driver != nullptr)
    {
        printf("Using PC sound driver: %s\n", pcsound_driver->name);
        return 1;
    }
    else
    {
        printf("Failed to find a working PC sound driver.\n");
        return 0;
    }
}

void PCSound_Shutdown()
{
    pcsound_driver->shutdown_func();
    pcsound_driver = nullptr;
}

