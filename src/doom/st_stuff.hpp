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
//	Status bar code.
//	Does the face/direction indicator animatin.
//	Does palette indicators as well (red pain/berserk, bright pickup)
//

#ifndef __STSTUFF_H__
#define __STSTUFF_H__

#include "doomtype.hpp"
#include "d_event.hpp"
#include "m_cheat.hpp"

// Size of statusbar.
// Now sensitive for scaling.
#define ST_HEIGHT 32
#define ST_WIDTH  ORIGWIDTH
#define ST_Y      (ORIGHEIGHT - ST_HEIGHT)

#define CRISPY_HUD 12

// [crispy] Demo Timer widget
extern void ST_DrawDemoTimer(const int time);
extern int  defdemotics, deftotaldemotics;

//
// STATUS BAR
//

// Called by main loop.
boolean ST_Responder(event_t *ev);

// Called by main loop.
void ST_Ticker();

// Called by main loop.
void ST_Drawer(boolean fullscreen, boolean refresh);

// Called when the console player is spawned on each level.
void ST_Start();

// Called by startup code.
void ST_Init();

// [crispy] forcefully initialize the status bar backing screen
extern void ST_refreshBackground(boolean force);


// States for status bar code.
typedef enum
{
    AutomapState,
    FirstPersonState

} st_stateenum_t;


// States for the chat code.
typedef enum
{
    StartChatState,
    WaitDestState,
    GetChatState

} st_chatstateenum_t;


extern pixel_t *  st_backing_screen;
extern cheatseq_t cheat_mus;
extern cheatseq_t cheat_god;
extern cheatseq_t cheat_ammo;
extern cheatseq_t cheat_ammonokey;
extern cheatseq_t cheat_noclip;
extern cheatseq_t cheat_commercial_noclip;
extern cheatseq_t cheat_powerup[8]; // [crispy] idbehold0
extern cheatseq_t cheat_choppers;
extern cheatseq_t cheat_clev;
extern cheatseq_t cheat_mypos;


#endif
