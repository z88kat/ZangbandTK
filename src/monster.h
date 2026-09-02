/**
 * \file monster.h
 * \brief Flags, structures and variables for monsters
 *
 * Copyright (c) 2007 Andi Sidwell
 * Copyright (c) 2010 Chris Carr
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */
#ifndef MONSTER_MONSTER_H
#define MONSTER_MONSTER_H

#include "h-basic.h"
#include "z-bitflag.h"
#include "z-rand.h"
#include "cave.h"
#include "target.h"
#include "mon-timed.h"
#include "mon-blows.h"

/*** Monster flags ***/

/**
 * Special Monster Flags (all temporary)
 */
enum
{
	#define MFLAG(a, b) MFLAG_##a,
	#include "list-mon-temp-flags.h"
	#undef MFLAG
	MFLAG_MAX
};

#define MFLAG_SIZE                FLAG_SIZE(MFLAG_MAX)

#define mflag_has(f, flag)        flag_has_dbg(f, MFLAG_SIZE, flag, #f, #flag)
#define mflag_next(f, flag)       flag_next(f, MFLAG_SIZE, flag)
#define mflag_is_empty(f)         flag_is_empty(f, MFLAG_SIZE)
#define mflag_is_full(f)          flag_is_full(f, MFLAG_SIZE)
#define mflag_is_inter(f1, f2)    flag_is_inter(f1, f2, MFLAG_SIZE)
#define mflag_is_subset(f1, f2)   flag_is_subset(f1, f2, MFLAG_SIZE)
#define mflag_is_equal(f1, f2)    flag_is_equal(f1, f2, MFLAG_SIZE)
#define mflag_on(f, flag)         flag_on_dbg(f, MFLAG_SIZE, flag, #f, #flag)
#define mflag_off(f, flag)        flag_off(f, MFLAG_SIZE, flag)
#define mflag_wipe(f)             flag_wipe(f, MFLAG_SIZE)
#define mflag_setall(f)           flag_setall(f, MFLAG_SIZE)
#define mflag_negate(f)           flag_negate(f, MFLAG_SIZE)
#define mflag_copy(f1, f2)        flag_copy(f1, f2, MFLAG_SIZE)
#define mflag_union(f1, f2)       flag_union(f1, f2, MFLAG_SIZE)
#define mflag_inter(f1, f2)       flag_inter(f1, f2, MFLAG_SIZE)
#define mflag_diff(f1, f2)        flag_diff(f1, f2, MFLAG_SIZE)

/**
 * Monster property and ability flags (race flags)
 */
enum
{
	#define RF(a, b, c) RF_##a,
	#include "list-mon-race-flags.h"
	#undef RF
	RF_MAX
};


/**
 * Categories for the monster race flags
 */
enum monster_flag_type {
	RFT_NONE = 0,	/* placeholder flag */
	RFT_OBV,		/* an obvious property */
	RFT_DISP,		/* for display purposes */
	RFT_GEN,		/* related to generation */
	RFT_NOTE,		/* especially noteworthy for lore */
	RFT_BEHAV,		/* behaviour-related */
	RFT_DROP,		/* drop details */
	RFT_DET,		/* detection properties */
	RFT_ALTER,		/* environment shaping */
	RFT_RACE_N,		/* types of monster (noun) */
	RFT_RACE_A,		/* types of monster (adjective) */
	RFT_VULN,		/* vulnerabilities with no corresponding resistance */
	RFT_VULN_I,		/* vulnerabilities with a corresponding resistance */
	RFT_RES,		/* elemental resistances */
	RFT_PROT,		/* immunity from status effects */

	RFT_MAX
};

#define RF_SIZE                FLAG_SIZE(RF_MAX)

#define rf_has(f, flag)        flag_has_dbg(f, RF_SIZE, flag, #f, #flag)
#define rf_next(f, flag)       flag_next(f, RF_SIZE, flag)
#define rf_count(f)            flag_count(f, RF_SIZE)
#define rf_is_empty(f)         flag_is_empty(f, RF_SIZE)
#define rf_is_full(f)          flag_is_full(f, RF_SIZE)
#define rf_is_inter(f1, f2)    flag_is_inter(f1, f2, RF_SIZE)
#define rf_is_subset(f1, f2)   flag_is_subset(f1, f2, RF_SIZE)
#define rf_is_equal(f1, f2)    flag_is_equal(f1, f2, RF_SIZE)
#define rf_on(f, flag)         flag_on_dbg(f, RF_SIZE, flag, #f, #flag)
#define rf_off(f, flag)        flag_off(f, RF_SIZE, flag)
#define rf_wipe(f)             flag_wipe(f, RF_SIZE)
#define rf_setall(f)           flag_setall(f, RF_SIZE)
#define rf_negate(f)           flag_negate(f, RF_SIZE)
#define rf_copy(f1, f2)        flag_copy(f1, f2, RF_SIZE)
#define rf_union(f1, f2)       flag_union(f1, f2, RF_SIZE)
#define rf_comp_union(f1, f2)  flag_comp_union(f1, f2, RF_SIZE)
#define rf_inter(f1, f2)       flag_inter(f1, f2, RF_SIZE)
#define rf_diff(f1, f2)        flag_diff(f1, f2, RF_SIZE)


/**
 * Monster spell flag indices
 */
enum
{
    #define RSF(a, b) RSF_##a,
    #include "list-mon-spells.h"
    #undef RSF
};

#define RSF_SIZE               FLAG_SIZE(RSF_MAX)


/** Structures **/

/**
 * The monster flag structure
 */
struct monster_flag {
	uint16_t index;			/* the RF_ index */
	uint16_t type;			/* RFT_ category */
	const char *desc;		/* lore description */
};

/**
 * Monster blows
 */
struct monster_blow {
	struct monster_blow *next;	/* Unused after parsing */

	struct blow_method *method;	/* Method */
	struct blow_effect *effect;	/* Effect */
	random_value dice;			/* Damage dice */
	int times_seen;				/* Sightings of the blow (lore only) */
};

/**
 * Monster pain messages
 */
struct monster_pain {
	const char *messages[7];
	int pain_idx;
	
	struct monster_pain *next;
};


/**
 * Monster spell levels
 */
struct monster_spell_level {
	struct monster_spell_level *next;

	int power;				/* Spell power at which this level starts */
	char *lore_desc;		/* Description of the attack used in lore text */
	uint8_t lore_attr;		/* Color of the attack used in lore text */
	uint8_t lore_attr_resist;	/* Color used in lore text when resisted */
	uint8_t lore_attr_immune;	/* Color used in lore text when resisted strongly */
	char *message;			/* Description of the attack */
	char *blind_message;	/* Description of the attack if unseen */
	char *miss_message;		/* Description of a missed attack */
	char *save_message;		/* Message on passing saving throw, if any */
};

/**
 * Monster spell types
 */
struct monster_spell {
	struct monster_spell *next;

	uint16_t index;				/* Numerical index (RSF_FOO) */
	int msgt;				/* Flag for message colouring */
	int hit;				/* To-hit level for the attack */
	struct effect *effect;	/* Effect(s) of the spell */
	struct monster_spell_level *level;	/* Spell power dependent details */
};


/**
 * Alternate spell message for a particular monster.
 */
enum monster_altmsg_type {
	MON_ALTMSG_SEEN,
	MON_ALTMSG_UNSEEN,
	MON_ALTMSG_MISS
};
struct monster_altmsg {
	struct monster_altmsg *next;

	char *message;				/* The alternate text;
							"" for no message */
	enum monster_altmsg_type msg_type;	/* Which of the spell's messages
							to override */
	uint16_t index;				/* The spell's numerical
							index (RSF_FOO) */
};


/**
 * Base monster type
 */
struct monster_base {
	struct monster_base *next;

	char *name;			/* Name for recognition in code */
	char *text;			/* In-game name */
	bitflag flags[RF_SIZE];         /* Flags */
	wchar_t d_char;			/* Default monster character */
	struct monster_pain *pain;	/* Pain messages */
};


/**
 * Specified monster drops
 */
struct monster_drop {
	struct monster_drop *next;
	struct object_kind *kind;
	unsigned int tval;
	unsigned int percent_chance;
	unsigned int min;
	unsigned int max;
};

enum monster_group_role {
	MON_GROUP_LEADER,
	MON_GROUP_SERVANT,
	MON_GROUP_BODYGUARD,
	MON_GROUP_MEMBER,
	MON_GROUP_SUMMON
};

/**
 * Monster friends (specific monster)
 */
struct monster_friends {
	struct monster_friends *next;
	char *name;
	struct monster_race *race;
	enum monster_group_role role;
	unsigned int percent_chance;
	unsigned int number_dice;
	unsigned int number_side;
};

/**
 * Monster friends (general type)
 */
struct monster_friends_base {
	struct monster_friends_base *next;
	struct monster_base *base;
	enum monster_group_role role;
	unsigned int percent_chance;
	unsigned int number_dice;
	unsigned int number_side;
};

/**
 * Monster group info
 */
struct monster_group_info {
	int index;
	enum monster_group_role role;
};

enum monster_group_type {
	PRIMARY_GROUP,
	SUMMON_GROUP,
	GROUP_MAX
};

/**
 * How monsters mimic
 */
struct monster_mimic {
	struct monster_mimic *next;
	struct object_kind *kind;
};

/**
 * Different shapes a monster can take
 */
struct monster_shape {
	struct monster_shape *next;
	char *name;
	struct monster_race *race;
	struct monster_base *base;
};

/**
 * Monster "race" information, including racial memories
 *
 * Note that "d_attr" and "d_char" are used for MORE than "visual" stuff.
 *
 * Note that "cur_num" (and "max_num") represent the number of monsters
 * of the given race currently on (and allowed on) the current level.
 * This information yields the "dead" flag for Unique monsters.
 *
 * Note that "max_num" is reset when a new player is created.
 * Note that "cur_num" is reset when a new level is created.
 *
 * Maybe "cur_num", and "max_num" should be moved out of this array since
 * they are not read from "monster.txt".
 */
struct monster_race {
	struct monster_race *next;

	unsigned int ridx;

	char *name;
	char *text;
	char *plural;			/* Optional pluralized name */

	struct monster_base *base;
	
	int avg_hp;				/* Average HP for this creature */

	int ac;					/* Armour Class */

	int sleep;				/* Inactive counter (base) */
	int hearing;			/* Monster sense of hearing (1-100, standard 20) */
	int smell;				/* Monster sense of smell (0-50, standard 20) */
	int speed;				/* Speed (normally 110) */
	int light;				/* Light intensity */

	int mexp;				/* Exp value for kill */

	int freq_innate;		/* Innate spell frequency */
	int freq_spell;			/* Other spell frequency */
	int spell_power;		/* Power of spells */

	bitflag flags[RF_SIZE];         /* Flags */
	bitflag spell_flags[RSF_SIZE];  /* Spell flags */

	struct monster_blow *blow; /* Melee blows */

	int level;				/* Level of creature */
	int rarity;				/* Rarity of creature */

	uint8_t d_attr;			/* Default monster attribute */
	wchar_t d_char;			/* Default monster character */

	uint8_t max_num;		/* Maximum population allowed per level */
	int cur_num;			/* Monster population on current level */

	struct monster_altmsg *spell_msgs;
	struct monster_drop *drops;

	struct monster_friends *friends;
	struct monster_friends_base *friends_base;

	struct monster_mimic *mimic_kinds;

	struct monster_shape *shapes;
	int num_shapes;
};


/**
 * Monster information, for a specific monster.
 *
 * Note: fy, fx constrain dungeon size to 256x256
 *
 * The "held_obj" field points to the first object of a stack
 * of objects (if any) being carried by the monster (see above).
 */
/**
 * Whose side a monster is on.
 *
 * ZangbandTK (PLR-22, PLR-29).  Angband 4.2 has no such notion: every monster
 * is an enemy by construction, and that assumption is built into targeting,
 * goal selection, projection and combat.
 *
 * Three states rather than a `friendly` boolean, because Zangband's three
 * behave differently and PLR-29 says so in terms.  A friendly monster will not
 * attack the player, cannot be commanded and is free; a pet can be commanded
 * and costs mana upkeep.  Collapsing them would make one of the two
 * unreachable.
 *
 * Zangband itself carried this as two bits stolen inside `m_ptr->smart`, the
 * smart-learn bitfield -- `SM_PET` and `SM_FRIENDLY`, both marked XXX
 * ([defines.h:2537](../archive/zangband/src/defines.h#L2537)) -- with hostile
 * derived as neither.  `set_pet()` did not clear `SM_FRIENDLY` and
 * `set_friendly()` did not clear `SM_PET`, so both could be set at once and
 * whichever test came first won.  An enum makes that state unrepresentable.
 *
 * HOSTILE is deliberately zero.  Every monster in the game is created through
 * `mem_zalloc` or an assignment from a zeroed struct, so nothing has to be
 * told to be hostile and no creation path can forget to be.
 */
enum monster_allegiance {
	MON_ALLEGIANCE_HOSTILE = 0,
	MON_ALLEGIANCE_FRIENDLY,
	MON_ALLEGIANCE_PET,
	MON_ALLEGIANCE_MAX
};

struct monster {
	struct monster_race *race;		/* Monster's (current) race */
	struct monster_race *original_race;	/* Changed monster's original race */
	int midx;

	struct loc grid;			/* Location on map */

	int16_t hp;				/* Current Hit points */
	int16_t maxhp;				/* Max Hit points */

	int16_t m_timed[MON_TMD_MAX];		/* Timed monster status effects */

	uint8_t mspeed;				/* Monster "speed" */
	uint8_t energy;				/* Monster "energy" */

	uint8_t cdis;				/* Current dis from player */

	bitflag mflag[MFLAG_SIZE];		/* Temporary monster flags */

	struct object *mimicked_obj;		/* Object this monster is mimicking */
	struct object *held_obj;		/* Object being held (if any) */

	uint8_t attr;  				/* attr last used for drawing monster */

	struct player_state known_pstate;	/* Known player state */

	struct target target;			/* Monster target */

	struct monster_group_info group_info[GROUP_MAX];/* Monster group details */
	struct heatmap heatmap;			/* Monster location heatmap */

	uint8_t min_range;			/* What is the closest we want to be? */
	uint8_t best_range;			/* How close do we want to be? */

	enum monster_allegiance allegiance;	/* Whose side it is on (PLR-22) */
};

/** Variables **/

extern struct monster_pain *pain_messages;
extern struct monster_spell *monster_spells;
extern struct monster_base *rb_info;
extern struct monster_race *r_info;
extern const struct monster_race *ref_race;

#endif /* !MONSTER_MONSTER_H */
