/**
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband License":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */
#ifndef INCLUDED_BORG_TRAIT_H
#define INCLUDED_BORG_TRAIT_H

/*
 * must be included before ALLOW_BORG to avoid empty compilation unit
 */
#include "../angband.h"

#ifdef ALLOW_BORG

#include "borg-item.h"
#include "borg-trait-swap.h"

/*
 * Possible values of "goal"
 */
#define GOAL_KILL    1 /* Monsters */
#define GOAL_TAKE    2 /* Objects */
#define GOAL_MISC    3 /* Stores */
#define GOAL_DARK    4 /* Exploring */
#define GOAL_XTRA    5 /* Searching */
#define GOAL_BORE    6 /* Leaving */
#define GOAL_FLEE    7 /* Fleeing */
#define GOAL_VAULT   8 /* Vaults */
#define GOAL_RECOVER 9 /* Resting safely */
#define GOAL_DIGGING 10 /* Anti-summon Corridor */

/*
 * Player race constants (hard-coded by save-files, arrays, etc)
 */
#define RACE_HUMAN      0
#define RACE_HALF_ELF   1
#define RACE_ELF        2
#define RACE_HOBBIT     3
#define RACE_GNOME      4
#define RACE_DWARF      5
#define RACE_HALF_ORC   6
#define RACE_HALF_TROLL 7
#define RACE_DUNADAN    8
#define RACE_HIGH_ELF   9
#define RACE_KOBOLD     10
/*
 * ZangbandTK (BRG-10): the most spellbooks any class here carries.
 *
 * Angband's widest caster has eight; this game's have 28. See `amt_book`
 * below for what the old bound of nine did.
 */
#define BORG_MAX_BOOKS 32

#define MAX_RACES       11

enum borg_item_pos { BORG_INVEN = 1, BORG_EQUIP = 2, BORG_QUILL = 4 };

/* NOTE: This must exactly match the prefix_pref enums in borg-trait.c */
enum {
    BI_STR,
    BI_INT,
    BI_WIS,
    BI_DEX,
    BI_CON,
    BI_ASTR,
    BI_AINT,
    BI_AWIS,
    BI_ADEX,
    BI_ACON,
    BI_CSTR,
    BI_CINT,
    BI_CWIS,
    BI_CDEX,
    BI_CCON,
    BI_STR_INDEX,
    BI_INT_INDEX,
    BI_WIS_INDEX,
    BI_DEX_INDEX,
    BI_CON_INDEX,
    BI_SSTR,
    BI_SINT,
    BI_SWIS,
    BI_SDEX,
    BI_SCON,
    BI_CLASS,
    BI_LIGHT,
    BI_CURHP,
    BI_MAXHP,
    BI_HP_ADJ,
    BI_CURSP,
    BI_MAXSP,
    BI_SP_ADJ,
    BI_FAIL1,
    BI_FAIL2,
    BI_CLEVEL,
    BI_MAXCLEVEL,
    BI_ESP,
    BI_RECALL,
    BI_FOOD,
    BI_FOOD_HI,
    BI_FOOD_LO,
    BI_FOOD_CURE_CONF,
    BI_FOOD_CURE_BLIND,
    BI_SPEED,
    BI_GOLD,
    BI_MOD_MOVES,
    BI_DAM_RED,
    BI_SDIG,
    BI_FEATH,
    BI_REG,
    BI_SINV,
    BI_INFRA,
    BI_FAST_SHOTS,
    BI_DISP,
    BI_DISM,
    BI_DEV,
    BI_SAV,
    BI_STL,
    BI_SRCH,
    BI_THN,
    BI_THB,
    BI_THT,
    BI_DIG,
    BI_IFIRE,
    BI_IACID,
    BI_ICOLD,
    BI_IELEC,
    BI_IPOIS,
    BI_RFIRE,
    BI_RCOLD,
    BI_RELEC,
    BI_RACID,
    BI_RPOIS,
    BI_RFEAR,
    BI_RLITE,
    BI_RDARK,
    BI_RBLIND,
    BI_RCONF,
    BI_RSND,
    BI_RSHRD,
    BI_RNXUS,
    BI_RNTHR,
    BI_RKAOS,
    BI_RDIS,
    BI_HLIFE,
    BI_FRACT,
    BI_SRFIRE,
    BI_SRCOLD,
    BI_SRELEC,
    BI_SRACID,
    BI_SRPOIS,
    BI_SRFEAR,
    BI_SRLITE,
    BI_SRDARK,
    BI_SRBLIND,
    BI_SRCONF,
    BI_SRSND,
    BI_SRSHRD,
    BI_SRNXUS,
    BI_SRNTHR,
    BI_SRKAOS,
    BI_SRDIS,
    BI_SHLIFE,
    BI_SFRACT,

    BI_CDEPTH,
    BI_MAXDEPTH,
    BI_KING,

    BI_ISWEAK,
    BI_ISHUNGRY,
    BI_ISFULL,
    BI_ISGORGED,
    BI_ISBLIND,
    BI_ISAFRAID,
    BI_ISCONFUSED,
    BI_ISPOISONED,
    BI_ISCUT,
    BI_ISSTUN,
    BI_ISHEAVYSTUN,
    BI_ISPARALYZED,
    BI_ISIMAGE,
    BI_ISFORGET,
    BI_ISENCUMB,
    BI_ISSTUDY,
    BI_ISFIXLEV,
    BI_ISFIXEXP,
    BI_HASFIXEXP,
    BI_ISFIXSTR,
    BI_ISFIXINT,
    BI_ISFIXWIS,
    BI_ISFIXDEX,
    BI_ISFIXCON,
    BI_ISFIXALL,

    BI_ARMOR,
    BI_TOHIT,
    BI_TODAM,
    BI_WTOHIT,
    BI_WTODAM,
    BI_WID,
    BI_WDD,
    BI_WDS,
    BI_BID,
    BI_BTOHIT,
    BI_BTODAM,
    BI_SLING,
    BI_BART,
    BI_BLOWS,
    BI_EXTRA_BLOWS,
    BI_SHOTS,
    BI_HEAVYWEPON,
    BI_HEAVYBOW,
    BI_AMMO_COUNT,
    BI_AMMO_TVAL,
    BI_AMMO_SIDES,
    BI_AMMO_POWER,
    BI_AMISSILES,
    BI_AMISSILES_SPECIAL,
    BI_AMISSILES_CURSED,
    BI_QUIVER_SLOTS,
    BI_FIRST_CURSED,
    BI_WHERE_CURSED,

    BI_CRSENVELOPING,
    BI_CRSIRRITATION,
    BI_CRSTELE,
    BI_CRSPOIS,
    BI_CRSSIREN,
    BI_CRSHALU,
    BI_CRSPARA,
    BI_CRSSDEM,
    BI_CRSSDRA,
    BI_CRSSUND,
    BI_CRSSTONE,
    BI_CRSNOTEL,
    BI_CRSTWEP,
    BI_CRSAGRV,
    BI_CRSVULN,
    BI_CRSDULL,
    BI_CRSSICK,
    BI_CRSWEAK,
    BI_CRSCLUM,
    BI_CRSSLOW,
    BI_CRSANNOY,
    BI_CRSHPIMP,
    BI_CRSMPIMP,
    BI_CRSSTEELSKIN,
    BI_CRSAIRSWING,
    BI_CRSFEAR,
    BI_CRSDRAIN_XP,
    BI_CRSFVULN,
    BI_CRSEVULN,
    BI_CRSCVULN,
    BI_CRSAVULN,
    BI_CRSUNKNO,

    BI_WS_ANIMAL,
    BI_WS_EVIL,
    BI_WS_UNDEAD,
    BI_WS_DEMON,
    BI_WS_ORC,
    BI_WS_TROLL,
    BI_WS_GIANT,
    BI_WS_DRAGON,
    BI_WK_UNDEAD,
    BI_WK_DEMON,
    BI_WK_DRAGON,
    BI_W_IMPACT,
    BI_WB_ACID,
    BI_WB_ELEC,
    BI_WB_FIRE,
    BI_WB_COLD,
    BI_WB_POIS,
    BI_APHASE,
    BI_ATELEPORT,
    BI_AESCAPE,
    BI_AFUEL,
    BI_AHEAL,
    BI_AEZHEAL,
    BI_ALIFE,
    BI_AID,
    BI_ASPEED,
    BI_ASTFMAGI,
    BI_ASTFDEST,
    BI_ATPORTOTHER,
    BI_ACUREPOIS,
    BI_ADETTRAP,
    BI_ADETDOOR,
    BI_ADETEVIL,
    BI_AMAGICMAP,
    BI_ARECHARGE,
    BI_ALITE,
    BI_APFE,
    BI_AGLYPH,
    BI_ACCW,
    BI_ACSW,
    BI_ACLW,
    BI_AENCH_TOH,
    BI_AENCH_TOD,
    BI_AENCH_SWEP,
    BI_AENCH_ARM,
    BI_AENCH_SARM,
    BI_ABRAND,
    BI_NEED_ENCHANT_TO_A,
    BI_NEED_ENCHANT_TO_H,
    BI_NEED_ENCHANT_TO_D,
    BI_NEED_BRAND_WEAPON,
    BI_ARESHEAT,
    BI_ARESCOLD,
    BI_ARESPOIS,
    BI_ATELEPORTLVL, /* scroll of teleport level */
    BI_AHWORD, /* Holy Word prayer */
    BI_AMASSBAN, /* ?Mass Banishment */
    BI_ASHROOM,
    BI_AROD1, /* Attack rods */
    BI_AROD2, /* Attack rods */
    BI_WORN_NEED_ID,
    BI_ALL_NEED_ID,
    BI_ADIGGER,
    BI_GOOD_S_CHG,
    BI_GOOD_W_CHG,
    BI_MULTIPLE_BONUSES,
    BI_DINV, /* See Inv Spell Legal */
    BI_WEIGHT, /* weight of all inventory and equipment */
    BI_CARRY, /* carry capacity */
    BI_EMPTY, /* number of empty slots */
    BI_SAURON_DEAD,
    BI_PREP_BIG_FIGHT,

    BI_MAX
};

struct borg_best
{
    bool    home;
    uint8_t tval; /* Item type */
    uint8_t sval; /* Item sub-type */
    int16_t pval; /* Item extra-info */
};

struct goals {
    /* goals */
    int16_t type; /* Flowing (goal type) */

    struct loc g; /* Goal location */

    bool rising; /* returning to town */
    bool leaving; /* leaving the level */
    bool fleeing; /* fleeing the level */
    bool fleeing_lunal; /* fleeing the level in lunal */
    bool fleeing_munchkin; /* Fleeing level while in munchkin Mode */
    bool fleeing_to_town; /* Fleeing the level to town */
    bool ignoring; /* ignoring monsters */
    bool less; /* return to, but don't use, the next up stairs */
    bool waiting; /* waiting for an approaching monster */

    int recalling; /* waiting for recall, guessing turns left */
    int descending; /* waiting for deep descent */

    int16_t shop; /* Next shop to visit */
    int16_t ware; /* Next item to buy there */
    int16_t item; /* Next item to sell there */

    bool    do_best;
    struct borg_best *best_item;

    /*
     * ZangbandTK (BRG-13): a destination in the *world*, not on this level.
     *
     * Every other goal here is a grid in the current chunk, which is right
     * when every level is a dungeon. The surface is a 144x144 window onto a
     * world of roughly fourteen windows by fourteen, and it is rebuilt and
     * re-anchored as the character crosses it (`wild_adopt_window()`), so a
     * level grid stops meaning anything the moment the window moves. Held in
     * world coordinates it survives the rebuild, which is the whole trick.
     *
     * Needed because there is no route to depth 30 that does not cross the
     * world: the Vaults of Amber ends at 15, the town staircase always leads
     * to the shallowest dungeon there is, and not one of the thirteen dungeon
     * mouths is inside the starting window -- the nearest reaching past 15 is
     * 576 grids away.
     *
     * `world_kind` says which sort of landmark is being walked to and
     * `world_index` identifies it within that sort; `BORG_WORLD_NONE` is
     * "not crossing".
     * `world_tries` is the step budget: a walk that stops closing the distance
     * is abandoned rather than retried, because a borg that re-picks an
     * unreachable target every time it gets bored is the same thrash as the
     * stair-scum loop and the bravery escalation.
     */
    struct loc world;
    int16_t    world_kind;      /* enum borg_world_goal */
    int16_t    world_index;     /* mouth or town, by kind */
    int16_t    world_tries;
    int16_t    world_best;
};

/**
 * What the borg is walking across the world to reach (BRG-13, BRG-25).
 *
 * A dungeon mouth was the only landmark worth crossing to while the borg's
 * whole purpose was depth. It is not: the starting village keeps four shops by
 * design (WLD-11a), and the borg's own restock rules want a Staff of
 * Teleportation from depth 10, which is a magic shop item. A borg that cannot
 * shop anywhere but where it was born stops at depth 9 whatever else is fixed.
 *
 * Both are the same walk to a different landmark, so they share everything
 * except the choosing and what happens on arrival -- a mouth is descended, a
 * town is simply stood in and the ordinary shopping takes over.
 */
enum borg_world_goal {
    BORG_WORLD_NONE = 0,
    BORG_WORLD_DUNGEON,
    BORG_WORLD_TOWN
};

struct temp {
    /* time stamps for processing see invisible */
    int16_t need_see_invis;
    int16_t see_inv;

    bool res_fire;
    bool res_cold;
    bool res_acid;
    bool res_elec;
    bool res_pois;

    bool prot_from_evil;
    bool fast;
    bool bless;
    bool hero;
    bool berserk;
    bool fastcast;
    bool regen;
    bool smite_evil;
    bool venom;
    bool shield;
};

/*
 * All the information the borg knows about itself
 */
struct borg_struct {
    /*
     * Where the surface window was anchored last time the map was read
     * (ZangbandTK, BRG-25). Window-relative memories -- the shop tracker --
     * are only meaningful while this has not moved.
     */
    struct loc wild_offset;

    struct player *player; /* !HACK to work around a MSVC bug */

    /* current traits, set in borg_notice */
    int *trait;
    /* items the borg is carrying or wearing */
    int *has;
    /* activations for artifacts the borg has */
    int *activation;

    /* how powerful the borg thinks it is set in borg_power */
    int32_t power;

    /* Current location */
    struct loc c;

    /* hit points last game turn to track change in hp */
    int16_t oldchp;

    /* activity flags */
    bool lunal_mode;
    bool munchkin_mode;

    bool stair_less; /* Use the next "up" staircase */
    bool stair_more; /* Use the next "down" staircase */

    bool in_shop;

    /* a 3 state boolean */
    /*-1 = not checked yet */
    /* 0 = not ready */
    /* 1 = ready */
    int ready_morgoth;

    struct temp temp;
    /* time stamps for processing see invisible */
    int16_t need_see_invis;
    int16_t see_inv;

    /* shifting the view (current panel) */
    bool    need_shift_panel; /* to spot off-screens */
    int16_t when_shift_panel;
    int16_t time_this_panel; /* Current "time" on current panel*/

    /* activity flags with time */
    int16_t no_retreat; /* amount of time to not retreat */
    int16_t resistance; /* borg is Resistant to all elements */
    int16_t when_call_light; /* When we last did call light */
    int16_t when_wizard_light; /* When we last did wizard light */
    int16_t when_detect_traps; /* When we last detected traps */
    int16_t when_detect_doors; /* When we last detected doors */
    int16_t when_detect_walls; /* When we last detected walls */
    int16_t when_detect_evil; /* When we last detected evil */
    int16_t when_detect_obj; /* When we last detected objects */
    int16_t when_last_kill_mult; /* When a multiplier was last killed */

    int16_t no_rest_prep; /* borg won't rest for a few turns */

    int16_t times_twitch; /* how often twitchy on this level */
    int16_t escapes; /* how often teleported on this level */

    /* trying an unknown potion wand rod scroll etc */
    bool trying_unknown;

    /* goals */
    struct goals goal;

    /* number of books */
    /*
     * ZangbandTK (BRG-10): sized by the widest class, not by Angband's.
     *
     * These were `[9]`, which is one more than Angband's widest caster. M9
     * gave this game seven realms of four books, so a Mage, Priest,
     * Warrior-Mage and High-Mage each carry **28**, a Ranger 24, a Rogue 16
     * and a Monk 12 -- seven of the twelve casters past the end of the array.
     *
     * `borg_note_spell_books()` writes `borg.book_idx[book_num]` for every
     * book of the class it finds in the pack, so a Mage holding its twelfth
     * book wrote off the end of this structure and corrupted whatever followed
     * it. And `borg_browse()` only ever looked at the first eight, so any
     * spell in a later book could not be learned even when the write landed
     * somewhere harmless. That is why a Mage here studied nothing in four
     * thousand turns, cast nothing, fought with its fists and died at
     * character level one in every seed.
     *
     * `borg_init_cave()`'s pattern is followed rather than making these
     * dynamic: a generous ceiling, plus a startup check that refuses to run if
     * the data outgrows it (see `borg_init_spells`). 32 is the next multiple of
     * four above 28, so a class gaining an eighth realm still fits.
     */
    int16_t amt_book[BORG_MAX_BOOKS];
    /* location of books in inventory */
    int16_t book_idx[BORG_MAX_BOOKS];

    /* need add to stat potions */
    bool need_statgain[STAT_MAX];
    /* Stat potions in inventory*/
    int16_t amt_statgain[STAT_MAX];
};
extern struct borg_struct borg;

extern bool borg_simulate; /* Simulation flag */
extern bool borg_attacking; /* Simulation flag */

/* defense flags */
extern bool borg_on_glyph; /* borg is standing on a glyph of warding */
extern bool borg_create_door; /* borg is going to create doors */
extern bool borg_sleep_spell;
extern bool borg_sleep_spell_ii;
extern bool borg_crush_spell;
extern bool borg_slow_spell; /* borg is about to cast the spell */
extern bool borg_confuse_spell;
extern bool borg_fear_mon_spell;

extern int16_t borg_game_ratio; /* the ratio of borg time to game time */

/* array of the strings that match the BI_* values */
extern const char *prefix_pref[];

/* Mega-Hack - indices of the player classes */
#define CLASS_WARRIOR     0
#define CLASS_MAGE        1
#define CLASS_DRUID       2
#define CLASS_PRIEST      3
#define CLASS_NECROMANCER 4
#define CLASS_PALADIN     5
#define CLASS_ROGUE       6
#define CLASS_RANGER      7
#define CLASS_BLACKGUARD  8

#define MAX_CLASSES 9 /* Max # of classes 0 = warrior, 5 = Paladin */

/*
 * helper to determine if swaps are being used.
 */
extern bool borg_uses_swaps(void);

/*
 * Utility to calculate the number of blows an item will get
 */
extern int borg_calc_blows(borg_item *item);

/*
 * Extract various bonuses
 */
extern void borg_notice(bool notice_swap);

/*
 * Update the "frame" info from the screen
 */
extern void borg_notice_player(void);

extern void borg_trait_init(void);
extern void borg_trait_free(void);

#endif

#endif
