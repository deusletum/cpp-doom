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
//	System specific interface stuff.
//


#ifndef __R_DRAW__
#define __R_DRAW__


extern lighttable_t *dc_colormap[2];
extern int           dc_x;
extern int           dc_yl;
extern int           dc_yh;
extern fixed_t       dc_iscale;
extern fixed_t       dc_texturemid;
extern int           dc_texheight;
extern uint8_t *        dc_brightmap;

// first pixel in a column
extern uint8_t *dc_source;


// The span blitting interface.
// Hook in assembler or system specific BLT
//  here.
void R_DrawColumn();
void R_DrawColumnLow();

// The Spectre/Invisibility effect.
void R_DrawFuzzColumn();
void R_DrawFuzzColumnLow();

// [crispy] draw fuzz effect independent of rendering frame rate
void R_SetFuzzPosTic();
void R_SetFuzzPosDraw();

// Draw with color translation tables,
//  for player sprite rendering,
//  Green/Red/Blue/Indigo shirts.
void R_DrawTranslatedColumn();
void R_DrawTranslatedColumnLow();

void R_DrawTLColumn();
void R_DrawTLColumnLow();

void R_VideoErase(unsigned ofs,
    int                    count);

extern int ds_y;
extern int ds_x1;
extern int ds_x2;

extern lighttable_t *ds_colormap[2];
extern uint8_t *        ds_brightmap;

extern fixed_t ds_xfrac;
extern fixed_t ds_yfrac;
extern fixed_t ds_xstep;
extern fixed_t ds_ystep;

// start of a 64*64 tile image
extern uint8_t *ds_source;

extern uint8_t *translationtables;
extern uint8_t *dc_translation;


// Span blitting for rows, floor/ceiling.
// No Sepctre effect needed.
void R_DrawSpan();

// Low resolution mode, 160x200?
void R_DrawSpanLow();


void R_InitBuffer(int width,
    int               height);


// Initialize color translation tables,
//  for player rendering etc.
void R_InitTranslationTables();


// Rendering function.
void R_FillBackScreen();

// If the view size is not full screen, draws a border around it.
void R_DrawViewBorder();


#endif