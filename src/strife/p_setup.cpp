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
//	Do all the WAD I/O, get map description,
//	set up initial state and misc. LUTs.
//



#include <cmath>

#include "z_zone.hpp"

#include "deh_main.hpp"
#include "i_swap.hpp"
#include "m_argv.hpp"
#include "m_bbox.hpp"

#include "g_game.hpp"

#include "i_system.hpp"
#include "w_wad.hpp"

#include "doomdef.hpp"
#include "p_local.hpp"

#include "s_sound.hpp"

#include "doomstat.hpp"
#include "lump.hpp"
#include "memory.hpp"


void	P_SpawnMapThing (mapthing_t*	mthing);


//
// MAP related Lookup tables.
// Store VERTEXES, LINEDEFS, SIDEDEFS, etc.
//
int		numvertexes;
vertex_t*	vertexes;

int		numsegs;
seg_t*		segs;

int		numsectors;
sector_t*	sectors;

int		numsubsectors;
subsector_t*	subsectors;

int		numnodes;
node_t*		nodes;

int		numlines;
line_t*		lines;

int		numsides;
side_t*		sides;

static int      totallines;

// BLOCKMAP
// Created from axis aligned bounding box
// of the map, a rectangular array of
// blocks of size ...
// Used to speed up collision detection
// by spatial subdivision in 2D.
//
// Blockmap size.
int		bmapwidth;
int		bmapheight;	// size in mapblocks
short*		blockmap;	// int for larger maps
// offsets in blockmap are from here
short*		blockmaplump;		
// origin of block map
fixed_t		bmaporgx;
fixed_t		bmaporgy;
// for thing chains
mobj_t**	blocklinks;


// REJECT
// For fast sight rejection.
// Speeds up enemy AI by skipping detailed
//  LineOf Sight calculation.
// Without special effect, this could be
//  used as a PVS lookup as well.
//
uint8_t *rejectmatrix;


// Maintain single and multi player starting spots.
#define MAX_DEATHMATCH_STARTS	10

mapthing_t	deathmatchstarts[MAX_DEATHMATCH_STARTS];
mapthing_t*	deathmatch_p;
mapthing_t	playerstarts[MAXPLAYERS];

// haleyjd 08/24/10: [STRIFE] rift spots for player spawning
mapthing_t      riftSpots[MAXRIFTSPOTS];




//
// P_LoadVertexes
//
void P_LoadVertexes (int lump)
{
    uint8_t            *data;
    int			i;
    mapvertex_t*	ml;
    vertex_t*		li;

    // Determine number of lumps:
    //  total lump length / vertex record length.
    numvertexes = static_cast<int>(W_LumpLength(lump) / sizeof(mapvertex_t));

    // Allocate zone memory for buffer.
    vertexes = zmalloc<vertex_t *>(static_cast<unsigned long>(numvertexes) * sizeof(vertex_t), PU_LEVEL, 0);

    // Load data into cache.
    data = cache_lump_num<uint8_t *> (lump, PU_STATIC);
	
    ml = reinterpret_cast<mapvertex_t *>(data);
    li = vertexes;

    // Copy and convert vertex coordinates,
    // internal representation as fixed.
    for (i=0 ; i<numvertexes ; i++, li++, ml++)
    {
	li->x = SHORT(ml->x)<<FRACBITS;
	li->y = SHORT(ml->y)<<FRACBITS;
    }

    // Free buffer memory.
    W_ReleaseLumpNum(lump);
}



//
// P_LoadSegs
//
void P_LoadSegs (int lump)
{
    uint8_t            *data;
    int			i;
    mapseg_t*		ml;
    seg_t*		li;
    line_t*		ldef;
    int                 linedef_local;
    int			side;
    int                 sidenum;
	
    numsegs = static_cast<int>(W_LumpLength(lump) / sizeof(mapseg_t));
    segs = zmalloc<seg_t *>(static_cast<unsigned long>(numsegs) * sizeof(seg_t), PU_LEVEL, 0);
    memset (segs, 0, static_cast<unsigned long>(numsegs) *sizeof(seg_t));
    data = cache_lump_num<uint8_t *> (lump,PU_STATIC);
	
    ml = reinterpret_cast<mapseg_t *>(data);
    li = segs;
    for (i=0 ; i<numsegs ; i++, li++, ml++)
    {
	li->v1 = &vertexes[SHORT(ml->v1)];
	li->v2 = &vertexes[SHORT(ml->v2)];

	li->angle = static_cast<angle_t>((SHORT(ml->angle)) << 16);
	li->offset = (SHORT(ml->offset))<<16;
        linedef_local   = SHORT(ml->linedef);
	ldef = &lines[linedef_local];
	li->linedef = ldef;
	side = SHORT(ml->side);
	li->sidedef = &sides[ldef->sidenum[side]];
	li->frontsector = sides[ldef->sidenum[side]].sector;

        if (ldef-> flags & ML_TWOSIDED)
        {
            sidenum = ldef->sidenum[side ^ 1];

            // If the sidenum is out of range, this may be a "glass hack"
            // impassible window.  Point at side #0 (this may not be
            // the correct Vanilla behavior; however, it seems to work for
            // OTTAWAU.WAD, which is the one place I've seen this trick
            // used).

            if (sidenum < 0 || sidenum >= numsides)
            {
                sidenum = 0;
            }

            li->backsector = sides[sidenum].sector;
        }
        else
        {
	    li->backsector = 0;
        }
    }
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadSubsectors
//
void P_LoadSubsectors (int lump)
{
    uint8_t            *data;
    int			i;
    mapsubsector_t*	ms;
    subsector_t*	ss;
	
    numsubsectors = static_cast<int>(W_LumpLength(lump) / sizeof(mapsubsector_t));
    subsectors = zmalloc<subsector_t *>(static_cast<unsigned long>(numsubsectors) * sizeof(subsector_t), PU_LEVEL, 0);
    data = cache_lump_num<uint8_t *> (lump,PU_STATIC);
	
    ms = reinterpret_cast<mapsubsector_t *>(data);
    memset (subsectors,0, static_cast<unsigned long>(numsubsectors) *sizeof(subsector_t));
    ss = subsectors;
    
    for (i=0 ; i<numsubsectors ; i++, ss++, ms++)
    {
	ss->numlines = SHORT(ms->numsegs);
	ss->firstline = SHORT(ms->firstseg);
    }
	
    W_ReleaseLumpNum(lump);
}



//
// P_LoadSectors
//
void P_LoadSectors (int lump)
{
    uint8_t            *data;
    int			i;
    mapsector_t*	ms;
    sector_t*		ss;
	
    numsectors = static_cast<int>(W_LumpLength(lump) / sizeof(mapsector_t));
    sectors = zmalloc<sector_t *>(static_cast<unsigned long>(numsectors) * sizeof(sector_t), PU_LEVEL, 0);
    memset (sectors, 0, static_cast<unsigned long>(numsectors) *sizeof(sector_t));
    data = cache_lump_num<uint8_t *> (lump,PU_STATIC);
	
    ms = reinterpret_cast<mapsector_t *>(data);
    ss = sectors;
    for (i=0 ; i<numsectors ; i++, ss++, ms++)
    {
	ss->floorheight = SHORT(ms->floorheight)<<FRACBITS;
	ss->ceilingheight = SHORT(ms->ceilingheight)<<FRACBITS;
	ss->floorpic = static_cast<short>(R_FlatNumForName(ms->floorpic));
	ss->ceilingpic = static_cast<short>(R_FlatNumForName(ms->ceilingpic));
	ss->lightlevel = SHORT(ms->lightlevel);
	ss->special = SHORT(ms->special);
	ss->tag = SHORT(ms->tag);
	ss->thinglist = nullptr;
    }
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadNodes
//
void P_LoadNodes (int lump)
{
    uint8_t    *data;
    int		i;
    int		j;
    int		k;
    mapnode_t*	mn;
    node_t*	no;
	
    numnodes = static_cast<int>(W_LumpLength(lump) / sizeof(mapnode_t));
    nodes = zmalloc<node_t *>(static_cast<unsigned long>(numnodes) * sizeof(node_t), PU_LEVEL, 0);
    data = cache_lump_num<uint8_t *> (lump,PU_STATIC);
	
    mn = reinterpret_cast<mapnode_t *>(data);
    no = nodes;
    
    for (i=0 ; i<numnodes ; i++, no++, mn++)
    {
	no->x = SHORT(mn->x)<<FRACBITS;
	no->y = SHORT(mn->y)<<FRACBITS;
	no->dx = SHORT(mn->dx)<<FRACBITS;
	no->dy = SHORT(mn->dy)<<FRACBITS;
	for (j=0 ; j<2 ; j++)
	{
	    no->children[j] = SHORT(mn->children[j]);
	    for (k=0 ; k<4 ; k++)
		no->bbox[j][k] = SHORT(mn->bbox[j][k])<<FRACBITS;
	}
    }
	
    W_ReleaseLumpNum(lump);
}


//
// P_LoadThings
//
// haleyjd 08/24/10: [STRIFE]:
// * Added code to record rift spots
void P_LoadThings (int lump)
{
    uint8_t            *data;
    int			i;
    mapthing_t         *mt;
    mapthing_t          spawnthing;
    int			numthings;
//    bool		spawn;

    data = cache_lump_num<uint8_t *> (lump,PU_STATIC);
    numthings = static_cast<int>(W_LumpLength(lump) / sizeof(mapthing_t));

    mt = reinterpret_cast<mapthing_t *>(data);
    for (i=0 ; i<numthings ; i++, mt++)
    {
//        spawn = true;

        // Do not spawn cool, new monsters if !commercial
        // STRIFE-TODO: replace with isregistered stuff
        /*
        if (gamemode != commercial)
        {
            switch (SHORT(mt->type))
            {
            case 68:	// Arachnotron
            case 64:	// Archvile
            case 88:	// Boss Brain
            case 89:	// Boss Shooter
            case 69:	// Hell Knight
            case 67:	// Mancubus
            case 71:	// Pain Elemental
            case 65:	// Former Human Commando
            case 66:	// Revenant
            case 84:	// Wolf SS
                spawn = false;
                break;
            }
        }
        if (spawn == false)
            break;
        */

        // Do spawn all other stuff. 
        spawnthing.x = SHORT(mt->x);
        spawnthing.y = SHORT(mt->y);
        spawnthing.angle = SHORT(mt->angle);
        spawnthing.type = SHORT(mt->type);
        spawnthing.options = SHORT(mt->options);

        // haleyjd 08/24/2010: Special Strife checks
        if(spawnthing.type >= 118 && spawnthing.type < 128)
        {
            // initialize riftSpots
            int riftSpotNum = spawnthing.type - 118;
            riftSpots[riftSpotNum] = spawnthing;
            riftSpots[riftSpotNum].type = 1;
        }
        else if(spawnthing.type >= 9001 && spawnthing.type < 9011)
        {
            // STRIFE-TODO: mystery array of 90xx objects
        }
        else
            P_SpawnMapThing(&spawnthing);
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadLineDefs
// Also counts secret lines for intermissions.
//
void P_LoadLineDefs (int lump)
{
    uint8_t            *data;
    int			i;
    maplinedef_t*	mld;
    line_t*		ld;
    vertex_t*		v1;
    vertex_t*		v2;
	
    numlines = static_cast<int>(W_LumpLength(lump) / sizeof(maplinedef_t));
    lines = zmalloc<line_t *>(static_cast<unsigned long>(numlines) * sizeof(line_t), PU_LEVEL, 0);
    memset (lines, 0, static_cast<unsigned long>(numlines) *sizeof(line_t));
    data = cache_lump_num<uint8_t *> (lump,PU_STATIC);
	
    mld = reinterpret_cast<maplinedef_t *>(data);
    ld = lines;
    for (i=0 ; i<numlines ; i++, mld++, ld++)
    {
	ld->flags = SHORT(mld->flags);
	ld->special = SHORT(mld->special);
	ld->tag = SHORT(mld->tag);
	v1 = ld->v1 = &vertexes[SHORT(mld->v1)];
	v2 = ld->v2 = &vertexes[SHORT(mld->v2)];
	ld->dx = v2->x - v1->x;
	ld->dy = v2->y - v1->y;
	
	if (!ld->dx)
	    ld->slopetype = ST_VERTICAL;
	else if (!ld->dy)
	    ld->slopetype = ST_HORIZONTAL;
	else
	{
	    if (FixedDiv (ld->dy , ld->dx) > 0)
		ld->slopetype = ST_POSITIVE;
	    else
		ld->slopetype = ST_NEGATIVE;
	}
		
	if (v1->x < v2->x)
	{
	    ld->bbox[BOXLEFT] = v1->x;
	    ld->bbox[BOXRIGHT] = v2->x;
	}
	else
	{
	    ld->bbox[BOXLEFT] = v2->x;
	    ld->bbox[BOXRIGHT] = v1->x;
	}

	if (v1->y < v2->y)
	{
	    ld->bbox[BOXBOTTOM] = v1->y;
	    ld->bbox[BOXTOP] = v2->y;
	}
	else
	{
	    ld->bbox[BOXBOTTOM] = v2->y;
	    ld->bbox[BOXTOP] = v1->y;
	}

	ld->sidenum[0] = SHORT(mld->sidenum[0]);
	ld->sidenum[1] = SHORT(mld->sidenum[1]);

	if (ld->sidenum[0] != -1)
	    ld->frontsector = sides[ld->sidenum[0]].sector;
	else
	    ld->frontsector = 0;

	if (ld->sidenum[1] != -1)
	    ld->backsector = sides[ld->sidenum[1]].sector;
	else
	    ld->backsector = 0;
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadSideDefs
//
void P_LoadSideDefs (int lump)
{
    uint8_t            *data;
    int			i;
    mapsidedef_t*	msd;
    side_t*		sd;
	
    numsides = static_cast<int>(W_LumpLength(lump) / sizeof(mapsidedef_t));
    sides = zmalloc<side_t *>(static_cast<unsigned long>(numsides) * sizeof(side_t), PU_LEVEL, 0);
    memset (sides, 0, static_cast<unsigned long>(numsides) *sizeof(side_t));
    data = cache_lump_num<uint8_t *> (lump,PU_STATIC);
	
    msd = reinterpret_cast<mapsidedef_t *>(data);
    sd = sides;
    for (i=0 ; i<numsides ; i++, msd++, sd++)
    {
	sd->textureoffset = SHORT(msd->textureoffset)<<FRACBITS;
	sd->rowoffset = SHORT(msd->rowoffset)<<FRACBITS;
	sd->toptexture = static_cast<short>(R_TextureNumForName(msd->toptexture));
	sd->bottomtexture = static_cast<short>(R_TextureNumForName(msd->bottomtexture));
	sd->midtexture = static_cast<short>(R_TextureNumForName(msd->midtexture));
	sd->sector = &sectors[SHORT(msd->sector)];
    }

    W_ReleaseLumpNum(lump);
}


//
// P_LoadBlockMap
//
void P_LoadBlockMap (int lump)
{
    int i;
    int lumplen;

    lumplen = static_cast<int>(W_LumpLength(lump));
    int count = lumplen / 2;
	
    blockmaplump = zmalloc<short *>(static_cast<size_t>(lumplen), PU_LEVEL, nullptr);
    W_ReadLump(lump, blockmaplump);
    blockmap = blockmaplump + 4;

    // Swap all short integers to native byte ordering.
  
    for (i=0; i<count; i++)
    {
	blockmaplump[i] = SHORT(blockmaplump[i]);
    }
		
    // Read the header

    bmaporgx = blockmaplump[0]<<FRACBITS;
    bmaporgy = blockmaplump[1]<<FRACBITS;
    bmapwidth = blockmaplump[2];
    bmapheight = blockmaplump[3];
	
    // Clear out mobj chains

    size_t block_count = sizeof(*blocklinks) * static_cast<unsigned long>(bmapwidth) * static_cast<unsigned long>(bmapheight);
    blocklinks = zmalloc<mobj_t **>(block_count, PU_LEVEL, 0);
    memset(blocklinks, 0, block_count);
}



//
// P_GroupLines
// Builds sector line lists and subsector sector numbers.
// Finds block bounding boxes for sectors.
//
void P_GroupLines ()
{
    line_t**		linebuffer;
    int			i;
    int			j;
    line_t*		li;
    sector_t*		sector;
    subsector_t*	ss;
    seg_t*		seg;
    fixed_t		bbox[4];
    int			block;
	
    // look up sector number for each subsector
    ss = subsectors;
    for (i=0 ; i<numsubsectors ; i++, ss++)
    {
	seg = &segs[ss->firstline];
	ss->sector = seg->sidedef->sector;
    }

    // count number of lines in each sector
    li = lines;
    totallines = 0;
    for (i=0 ; i<numlines ; i++, li++)
    {
	totallines++;
	li->frontsector->linecount++;

	if (li->backsector && li->backsector != li->frontsector)
	{
	    li->backsector->linecount++;
	    totallines++;
	}
    }

    // build line tables for each sector	
    linebuffer = zmalloc<line_t **>(static_cast<unsigned long>(totallines) * sizeof(line_t *), PU_LEVEL, 0);

    for (i=0; i<numsectors; ++i)
    {
        // Assign the line buffer for this sector

        sectors[i].lines = linebuffer;
        linebuffer += sectors[i].linecount;

        // Reset linecount to zero so in the next stage we can count
        // lines into the list.

        sectors[i].linecount = 0;
    }

    // Assign lines to sectors

    for (i=0; i<numlines; ++i)
    { 
        li = &lines[i];

        if (li->frontsector != nullptr)
        {
            sector = li->frontsector;

            sector->lines[sector->linecount] = li;
            ++sector->linecount;
        }

        if (li->backsector != nullptr && li->frontsector != li->backsector)
        {
            sector = li->backsector;

            sector->lines[sector->linecount] = li;
            ++sector->linecount;
        }
    }
    
    // Generate bounding boxes for sectors
	
    sector = sectors;
    for (i=0 ; i<numsectors ; i++, sector++)
    {
	M_ClearBox (bbox);

	for (j=0 ; j<sector->linecount; j++)
	{
            li = sector->lines[j];

            M_AddToBox (bbox, li->v1->x, li->v1->y);
            M_AddToBox (bbox, li->v2->x, li->v2->y);
	}

	// set the degenmobj_t to the middle of the bounding box
	sector->soundorg.x = (bbox[BOXRIGHT]+bbox[BOXLEFT])/2;
	sector->soundorg.y = (bbox[BOXTOP]+bbox[BOXBOTTOM])/2;
		
	// adjust bounding box to map blocks
	block = (bbox[BOXTOP]-bmaporgy+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapheight ? bmapheight-1 : block;
	sector->blockbox[BOXTOP]=block;

	block = (bbox[BOXBOTTOM]-bmaporgy-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXBOTTOM]=block;

	block = (bbox[BOXRIGHT]-bmaporgx+MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block >= bmapwidth ? bmapwidth-1 : block;
	sector->blockbox[BOXRIGHT]=block;

	block = (bbox[BOXLEFT]-bmaporgx-MAXRADIUS)>>MAPBLOCKSHIFT;
	block = block < 0 ? 0 : block;
	sector->blockbox[BOXLEFT]=block;
    }
	
}

// Pad the REJECT lump with extra data when the lump is too small,
// to simulate a REJECT buffer overflow in Vanilla Doom.

static void PadRejectArray(uint8_t *array, unsigned int len)
{
    unsigned int i;
    unsigned int byte_num;
    uint8_t     *dest;
    unsigned int padvalue;

    // Values to pad the REJECT array with:

    unsigned int rejectpad[4] =
    {
        0,                                    // Size
        0,                                    // Part of z_zone block header
        50,                                   // PU_LEVEL
        0x1d4a11                              // DOOM_CONST_ZONEID
    };

    rejectpad[0] = static_cast<unsigned int>(((totallines * 4 + 3) & ~3) + 24);

    // Copy values from rejectpad into the destination array.

    dest = array;

    for (i=0; i<len && i<sizeof(rejectpad); ++i)
    {
        byte_num = i % 4;
        *dest = (rejectpad[i / 4] >> (byte_num * 8)) & 0xff;
        ++dest;
    }

    // We only have a limited pad size.  Print a warning if the
    // REJECT lump is too small.

    if (len > sizeof(rejectpad))
    {
        fprintf(stderr,
                "PadRejectArray: REJECT lump too short to pad! (%u > %i)\n",
                len, static_cast<int>(sizeof(rejectpad)));

        // Pad remaining space with 0 (or 0xff, if specified on command line).

        if (M_CheckParm("-reject_pad_with_ff"))
        {
            padvalue = 0xff;
        }
        else
        {
            padvalue = 0xf00;
        }

        memset(array + sizeof(rejectpad), static_cast<int>(padvalue), len - sizeof(rejectpad));
    }
}

static void P_LoadReject(int lumpnum)
{
    int minlength;
    int lumplen;

    // Calculate the size that the REJECT lump *should* be.

    minlength = (numsectors * numsectors + 7) / 8;

    // If the lump meets the minimum length, it can be loaded directly.
    // Otherwise, we need to allocate a buffer of the correct size
    // and pad it with appropriate data.

    lumplen = static_cast<int>(W_LumpLength(lumpnum));

    if (lumplen >= minlength)
    {
        rejectmatrix = cache_lump_num<uint8_t *>(lumpnum, PU_LEVEL);
    }
    else
    {
        rejectmatrix = zmalloc<uint8_t *>(static_cast<size_t>(minlength), PU_LEVEL, &rejectmatrix);
        W_ReadLump(lumpnum, rejectmatrix);

        PadRejectArray(rejectmatrix + lumplen, static_cast<unsigned int>(minlength - lumplen));
    }
}

//
// P_SetupLevel
//
void P_SetupLevel(int map, int, skill_t)
{
    int     i;
    char    lumpname[9];
    int     lumpnum;

    // haleyjd 20110205 [STRIFE]: removed totalitems and wminfo
    totalkills =  totalsecret = 0;

    for (i=0 ; i<MAXPLAYERS ; i++)
    {
        // haleyjd 20100830: [STRIFE] Removed secretcount, itemcount
        //         20110205: [STRIFE] Initialize players.allegiance
        players[i].allegiance = static_cast<short>(i);
        players[i].killcount = 0;
    }

    // Initial height of PointOfView
    // will be set by player think.
    players[consoleplayer].viewz = 1; 

    // Make sure all sounds are stopped before Z_FreeTags.
    S_Start ();

    
#if 0 // UNUSED
    if (debugfile)
    {
        Z_FreeTags (PU_LEVEL, INT_MAX);
        Z_FileDumpHeap (debugfile);
    }
    else
#endif
    Z_FreeTags (PU_LEVEL, PU_PURGELEVEL-1);


    // UNUSED W_Profile ();
    P_InitThinkers ();

    // if working with a devlopment map, reload it
    W_Reload();

    // [STRIFE] Removed ExMy map support
    if (map<10)
        DEH_snprintf(lumpname, 9, "map0%i", map);
    else
        DEH_snprintf(lumpname, 9, "map%i", map);

    lumpnum = W_GetNumForName (lumpname);

    leveltime = 0;

    // note: most of this ordering is important	
    P_LoadBlockMap (lumpnum+ML_BLOCKMAP);
    P_LoadVertexes (lumpnum+ML_VERTEXES);
    P_LoadSectors (lumpnum+ML_SECTORS);
    P_LoadSideDefs (lumpnum+ML_SIDEDEFS);

    P_LoadLineDefs (lumpnum+ML_LINEDEFS);
    P_LoadSubsectors (lumpnum+ML_SSECTORS);
    P_LoadNodes (lumpnum+ML_NODES);
    P_LoadSegs (lumpnum+ML_SEGS);

    P_GroupLines ();
    P_LoadReject (lumpnum+ML_REJECT);

    //bodyqueslot = 0; [STRIFE] unused
    deathmatch_p = deathmatchstarts;
    P_LoadThings (lumpnum+ML_THINGS);
    
    // if deathmatch, randomly spawn the active players
    if (deathmatch)
    {
        for (i=0 ; i<MAXPLAYERS ; i++)
            if (playeringame[i])
            {
                players[i].mo = nullptr;
                G_DeathMatchSpawnPlayer (i);
            }

    }

    // clear special respawning que
    iquehead = iquetail = 0;

    // set up world state
    P_SpawnSpecials ();

    // build subsector connect matrix
    // UNUSED P_ConnectSubsectors ();

    // preload graphics
    if (precache)
        R_PrecacheLevel ();

    //printf ("free memory: 0x%x\n", Z_FreeMemory());
}



//
// P_Init
//
void P_Init ()
{
    P_InitSwitchList();
    P_InitPicAnims();
    P_InitTerrainTypes();   // villsa [STRIFE]
    R_InitSprites(sprnames);
}



