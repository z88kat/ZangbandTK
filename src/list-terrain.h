/**
 * \file list-terrain.h
 * \brief List the terrain (feature) types that can appear
 *
 * These are how the code and data files refer to terrain.  Any changes will
 * break savefiles.  Note that the terrain code is stored as an unsigned 8-bit
 * integer so there can be at most 256 types of terrain.  Flags below start
 * from zero on line 13, so a terrain's sequence number is its line number
 * minus 13.
 */

/* symbol */
FEAT(NONE) /* nothing/unknown */
FEAT(FLOOR) /* open floor */
FEAT(CLOSED) /* closed door */
FEAT(OPEN) /* open door */
FEAT(BROKEN) /* broken door */
FEAT(LESS) /* up staircase */
FEAT(MORE) /* down staircase */
FEAT(STORE_GENERAL)
FEAT(STORE_ARMOR)
FEAT(STORE_WEAPON)
FEAT(STORE_BOOK)
FEAT(STORE_ALCHEMY)
FEAT(STORE_MAGIC)
FEAT(STORE_BLACK)
FEAT(HOME)
FEAT(SECRET) /* secret door */
FEAT(RUBBLE) /* impassable rubble */
FEAT(MAGMA) /* magma vein wall */
FEAT(QUARTZ) /* quartz vein wall */
FEAT(MAGMA_K) /* magma vein wall with treasure */
FEAT(QUARTZ_K) /* quartz vein wall with treasure */
FEAT(GRANITE) /* granite wall */
FEAT(PERM) /* permanent wall */
FEAT(LAVA)
FEAT(PASS_RUBBLE)

/* ZangbandTK wilderness terrain (WLD-09) */
FEAT(GRASS)      /* open grassland */
FEAT(DIRT)       /* bare earth and waste */
FEAT(SAND)       /* shore and beach */
FEAT(TREE)       /* woodland; blocks sight */
FEAT(MUD)        /* swamp and marsh */
FEAT(WATER)      /* shallow water; wadeable */
FEAT(DEEP_WATER) /* open sea */
FEAT(ROCK)       /* mountainside; impassable */
FEAT(ROAD)       /* made road between places */
FEAT(WORLD_EDGE) /* the sea at the end of a flat world */
FEAT(DUNGEON)    /* the mouth of a named dungeon (WLD-14) */
FEAT(MAGETOWER)  /* a tower that carries you between places (WLD-16c) */
FEAT(HEALER)     /* somebody who will mend you, for gold (WLD-16c) */
FEAT(INN)        /* a bed until morning (WLD-16c) */
FEAT(MAGESMITH)  /* puts magic on an item (WLD-16c) */
FEAT(RECHARGER)  /* puts charges back in a wand (WLD-16c) */
FEAT(CHAOSTOWER) /* takes a mutation off you, for a price (DEC-24, PLR-13) */

/*
 * ZangbandTK (BAL-15): a wall that looks like floor, in nightmare mode.
 *
 * Appended, and the position is the point -- `save.c:989` writes a grid's
 * feature as a raw byte index, so inserting a feature beside the other walls
 * would shift every later one and every savefile in existence would come back
 * with its terrain wrong. The same trap PROT_CUT hit in the object flags
 * (DEC-79), one file over.
 */
FEAT(INVIS_WALL) /* looks like floor and is not (BAL-15, DEC-83) */
