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
//	Savegame I/O, archiving, persistence.
//


#ifndef __P_SAVEG__
#define __P_SAVEG__

#include <cstdio>

#define SAVEGAME_EOF 0x1d
#define VERSIONSIZE  16

// maximum size of a savegame description

#define SAVESTRINGSIZE 24

// temporary filename to use while saving.

char *P_TempSaveGameFile();

// filename to use for a savegame slot

char *P_SaveGameFile(int slot);

// Savegame file header read/write functions

bool P_ReadSaveGameHeader();
void    P_WriteSaveGameHeader(char *description);

// Savegame end-of-file read/write functions

bool P_ReadSaveGameEOF();
void    P_WriteSaveGameEOF();

// Persistent storage/archiving.
// These are the load / save game routines.
void P_ArchivePlayers();
void P_UnArchivePlayers();
void P_ArchiveWorld();
void P_UnArchiveWorld();
void P_ArchiveThinkers();
void P_UnArchiveThinkers();
void P_ArchiveSpecials();
void P_UnArchiveSpecials();
void P_RestoreTargets();

extern FILE *  save_stream;
extern bool savegame_error;


#endif
