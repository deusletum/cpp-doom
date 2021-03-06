//
// Copyright(C) 1993-1996 Id Software, Inc.
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
//  AutoMap module.
//

#ifndef __AMMAP_H__
#define __AMMAP_H__

#include "d_event.hpp"
#include "m_cheat.hpp"

// Used by ST StatusBar stuff.
constexpr auto AM_MSGHEADER  = (('a' << 24) + ('m' << 16));
constexpr auto AM_MSGENTERED = (AM_MSGHEADER | ('e' << 8));
constexpr auto AM_MSGEXITED  = (AM_MSGHEADER | ('x' << 8));


// Called by main loop.
bool AM_Responder(event_t *ev);

// Called by main loop.
void AM_Ticker();

// Called by main loop,
// called instead of view drawer if automap active.
void AM_Drawer();

// Called to force the automap to quit
// if the level is completed while it is up.
void AM_Stop();


extern cheatseq_t cheat_amap;


#endif
