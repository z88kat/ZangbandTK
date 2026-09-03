/**
 * \file init.c
 * \brief Various game initialization routines
 *
 * Copyright (c) 1997 Ben Harrison
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
 *
 * This file is used to initialize various variables and arrays for the
 * Angband game.
 *
 * Several of the arrays for Angband are built from data files in the
 * "lib/gamedata" directory.
 */


#include "angband.h"
#include "buildid.h"
#include "cave.h"
#include "cmds.h"
#include "cmd-core.h"
#include "datafile.h"
#include "effects.h"
#include "game-event.h"
#include "game-world.h"
#include "generate.h"
#include "hint.h"
#include "dun-type.h"
#include "init.h"
#include "player-mutation.h"
#include "player-virtue.h"
#include "message.h"
#include "mon-init.h"
#include "mon-list.h"
#include "mon-lore.h"
#include "mon-make.h"
#include "mon-msg.h"
#include "mon-speech.h"
#include "mon-summon.h"
#include "mon-util.h"
#include "monster.h"
#include "obj-chest.h"
#include "obj-ignore.h"
#include "obj-init.h"
#include "obj-list.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-power.h"
#include "obj-randart.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "object.h"
#include "option.h"
#include "player.h"
#include "player-history.h"
#include "player-quest.h"
#include "player-spell.h"
#include "player-timed.h"
#include "project.h"
#include "randname.h"
#include "store.h"
#include "trap.h"
#include "ui-entry.h"
#include "ui-entry-init.h"
#include "ui-visuals.h"
#include "wild.h"

bool play_again = false;

/**
 * Structure (not array) of game constants
 */
struct angband_constants *z_info;

/*
 * The special Angband "System Suffix"
 * This variable is used to choose an appropriate "pref-xxx" file
 */
const char *ANGBAND_SYS = "xxx";

/**
 * Various directories. These are no longer necessarily all subdirs of "lib"
 */
char *ANGBAND_DIR_GAMEDATA;
char *ANGBAND_DIR_CUSTOMIZE;
char *ANGBAND_DIR_HELP;
char *ANGBAND_DIR_SCREENS;
char *ANGBAND_DIR_FONTS;
char *ANGBAND_DIR_TILES;
char *ANGBAND_DIR_SOUNDS;
char *ANGBAND_DIR_ICONS;
char *ANGBAND_DIR_USER;
char *ANGBAND_DIR_SAVE;
char *ANGBAND_DIR_PANIC;
char *ANGBAND_DIR_SCORES;
char *ANGBAND_DIR_ARCHIVE;

static const char *slots[] = {
	#define EQUIP(a, b, c, d, e, f) #a,
	#include "list-equip-slots.h"
	#undef EQUIP
	NULL
};

const char *list_obj_flag_names[] = {
	"NONE",
	#define OF(a, b) #a,
	#include "list-object-flags.h"
	#undef OF
	NULL
};

static const char *obj_mods[] = {
	#define STAT(a) #a,
	#include "list-stats.h"
	#undef STAT
	#define OBJ_MOD(a) #a,
	#include "list-object-modifiers.h"
	#undef OBJ_MOD
	NULL
};

const char *list_element_names[] = {
	#define ELEM(a) #a,
	#include "list-elements.h"
	#undef ELEM
	NULL
};

static const char *effect_list[] = {
	"NONE",
	#define EFFECT(x, a, b, c, d, e, f) #x,
	#include "list-effects.h"
	#undef EFFECT
	"MAX"
};

static const char *trap_flags[] =
{
	#define TRF(a, b) #a,
	#include "list-trap-flags.h"
	#undef TRF
    NULL
};

static const char *terrain_flags[] =
{
	#define TF(a, b) #a,
	#include "list-terrain-flags.h"
	#undef TF
    NULL
};

static const char *mon_race_flags[] =
{
	#define RF(a, b, c) #a,
	#include "list-mon-race-flags.h"
	#undef RF
	NULL
};

static const char *player_info_flags[] =
{
	#define PF(a) #a,
	#include "list-player-flags.h"
	#undef PF
	NULL
};

errr grab_effect_data(struct parser *p, struct effect *effect)
{
	const char *type;
	int val;

	if (grab_name("effect", parser_getsym(p, "eff"), effect_list,
				  N_ELEMENTS(effect_list), &val))
		return PARSE_ERROR_INVALID_EFFECT;
	effect->index = val;

	if (parser_hasval(p, "type")) {
		type = parser_getsym(p, "type");

		if (type == NULL)
			return PARSE_ERROR_UNRECOGNISED_PARAMETER;

		/* Check for a value */
		val = effect_subtype(effect->index, type);
		if (val < 0)
			return PARSE_ERROR_INVALID_VALUE;
		else
			effect->subtype = val;
	}

	if (parser_hasval(p, "radius"))
		effect->radius = parser_getint(p, "radius");

	if (parser_hasval(p, "other"))
		effect->other = parser_getint(p, "other");

	return PARSE_ERROR_NONE;
}

static enum parser_error write_book_kind(struct class_book *book,
										 const char *name)
{
	struct object_kind *temp, *kind;
	int i;

	/* Check we haven't already made this book */
	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].name && streq(name, k_info[i].name)) {
			book->sval = k_info[i].sval;
			return PARSE_ERROR_NONE;
		}
	}

	/* Extend by 1 and realloc */
	z_info->k_max += 1;
	z_info->ordinary_kind_max += 1;
	temp = mem_realloc(k_info, (z_info->k_max + 1) * sizeof(*temp));

	/* Copy if no errors */
	if (!temp) {
		return PARSE_ERROR_INTERNAL;
	} else {
		k_info = temp;
	}

	/* Add this entry at the end */
	kind = &k_info[z_info->k_max - 1];
	memset(kind, 0, sizeof(*kind));

	/* Copy the tval and base */
	kind->tval = book->tval;
	kind->base = &kb_info[kind->tval];

	/* Make the name and index */
	kind->name = string_make(name);
	kind->kidx = z_info->k_max - 1;

	/* Increase the sval count for this tval, set the new one to the max */
	for (i = 0; i < TV_MAX; i++)
		if (kb_info[i].tval == kind->tval) {
			kb_info[i].num_svals++;
			kind->sval = kb_info[i].num_svals;
			break;
		}
	if (i == TV_MAX) return PARSE_ERROR_INTERNAL;

	/* Copy the sval to the artifact info */
	book->sval = kind->sval;

	/* Set object defaults (graphics should be overwritten) */
	kind->d_char = '*';
	kind->d_attr = COLOUR_RED;
	kind->dd = 1;
	kind->ds = 1;
	kind->weight = 30;

	/* Inherit base flags. */
	kf_union(kind->kind_flags, kb_info[kind->tval].kind_flags);

	/* Dungeon books get extra properties */
	if (book->dungeon) {
		for (i = ELEM_BASE_MIN; i < ELEM_BASE_MAX; i++) {
			kind->el_info[i].flags |= EL_INFO_IGNORE;
		}
		kf_on(kind->kind_flags, KF_GOOD);
	}

	return PARSE_ERROR_NONE;
}

/**
 * Find the default paths to all of our important sub-directories.
 *
 * All of the sub-directories should, for a single-user install, be
 * located inside the main directory, whose location is very system-dependent.
 * For shared installations, typically on Unix or Linux systems, the
 * directories may be scattered - see config.h for more info.
 *
 * This function takes buffers, holding the paths to the "config", "lib",
 * and "data" directories (for example, those could be "/etc/angband/",
 * "/usr/share/angband", and "/var/games/angband").  Some system-dependent
 * expansion/substitution may be done when copying those base paths to the
 * paths Angband uses:  see path_process() in z-file.c for details (Unix
 * implementations, for instance, try to replace a leading ~ or ~username with
 * the path to a home directory).
 *
 * Various command line options may allow some of the important
 * directories to be changed to user-specified directories, most
 * importantly, the "scores" and "user" and "save" directories,
 * but this is done after this function, see "main.c".
 *
 * In general, the initial path should end in the appropriate "PATH_SEP"
 * string.  All of the "sub-directory" paths (created below or supplied
 * by the user) will NOT end in the "PATH_SEP" string, see the special
 * "path_build()" function in "util.c" for more information.
 *
 * First we free all the strings, since this is known
 * to succeed even if the strings have not been allocated yet,
 * as long as the variables start out as "NULL".  This allows
 * this function to be called multiple times, for example, to
 * try several base "path" values until a good one is found.
 */
void init_file_paths(const char *configpath, const char *libpath, const char *datapath)
{
	char buf[1024];
	char *userpath = NULL;

	/*** Free everything ***/

	/* Free the sub-paths */
	string_free(ANGBAND_DIR_GAMEDATA);
	string_free(ANGBAND_DIR_CUSTOMIZE);
	string_free(ANGBAND_DIR_HELP);
	string_free(ANGBAND_DIR_SCREENS);
	string_free(ANGBAND_DIR_FONTS);
	string_free(ANGBAND_DIR_TILES);
	string_free(ANGBAND_DIR_SOUNDS);
	string_free(ANGBAND_DIR_ICONS);
	string_free(ANGBAND_DIR_USER);
	string_free(ANGBAND_DIR_SAVE);
	string_free(ANGBAND_DIR_PANIC);
	string_free(ANGBAND_DIR_SCORES);
	string_free(ANGBAND_DIR_ARCHIVE);

	/*** Prepare the paths ***/

#define BUILD_DIRECTORY_PATH(dest, basepath, dirname) { \
	path_build(buf, sizeof(buf), (basepath), (dirname)); \
	dest = string_make(buf); \
}

	/* Paths generally containing configuration data for Angband. */
#ifdef GAMEDATA_IN_LIB
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, libpath, "gamedata");
#else
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_GAMEDATA, configpath, "gamedata");
#endif
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_CUSTOMIZE, configpath, "customize");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_HELP, libpath, "help");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCREENS, libpath, "screens");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_FONTS, libpath, "fonts");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_TILES, libpath, "tiles");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SOUNDS, libpath, "sounds");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_ICONS, libpath, "icons");

#ifdef PRIVATE_USER_PATH

	/* Build the path to the user specific directory */
	if (strncmp(ANGBAND_SYS, "test", 4) == 0)
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, "Test");
	else
		path_build(buf, sizeof(buf), PRIVATE_USER_PATH, VERSION_NAME);
	ANGBAND_DIR_USER = string_make(buf);

#else /* !PRIVATE_USER_PATH */

#ifdef MACH_O_CARBON
	/* Remove any trailing separators, since some deeper path creation functions
	 * don't like directories with trailing slashes. */
	if (suffix(datapath, PATH_SEP)) {
		/* Hacky way to trim the separator. Since this is just for OS X, we can
		 * assume a one char separator. */
		int last_char_index = strlen(datapath) - 1;
		my_strcpy(buf, datapath, sizeof(buf));
		buf[last_char_index] = '\0';
		ANGBAND_DIR_USER = string_make(buf);
	}
	else {
		ANGBAND_DIR_USER = string_make(datapath);
	}
#else /* !MACH_O_CARBON */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_USER, datapath, "user");
#endif /* MACH_O_CARBON */

#endif /* PRIVATE_USER_PATH */

	/* Build the path to the archive directory. */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_ARCHIVE, ANGBAND_DIR_USER, "archive");

#ifdef USE_PRIVATE_PATHS
	userpath = ANGBAND_DIR_USER;
#else /* !USE_PRIVATE_PATHS */
	userpath = (char *)datapath;
#endif /* USE_PRIVATE_PATHS */

	/* Build the path to the score and save directories */
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SCORES, userpath, "scores");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_SAVE, userpath, "save");
	BUILD_DIRECTORY_PATH(ANGBAND_DIR_PANIC, userpath, "panic");

#undef BUILD_DIRECTORY_PATH
}


/**
 * Create any missing directories. We create only those dirs which may be
 * empty (user/, save/, scores/, info/, help/). The others are assumed
 * to contain required files and therefore must exist at startup
 * (edit/, pref/, file/, xtra/).
 *
 * ToDo: Only create the directories when actually writing files.
 */
void create_needed_dirs(void)
{
	char dirpath[512];

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_USER, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SAVE, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_PANIC, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_SCORES, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);

	path_build(dirpath, sizeof(dirpath), ANGBAND_DIR_ARCHIVE, "");
	if (!dir_create(dirpath)) quit_fmt("Cannot create '%s'", dirpath);
}

/**
 * ------------------------------------------------------------------------
 * Initialize game constants
 * ------------------------------------------------------------------------ */

static enum parser_error parse_constants_level_max(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "monsters"))
		z->level_monster_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_mon_gen(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "chance"))
		z->alloc_monster_chance = value;
	else if (streq(label, "level-min"))
		z->level_monster_min = value;
	else if (streq(label, "town-day"))
		z->town_monsters_day = value;
	else if (streq(label, "town-night"))
		z->town_monsters_night = value;
	else if (streq(label, "repro-max"))
		z->repro_monster_max = value;
	else if (streq(label, "ood-chance"))
		z->ood_monster_chance = value;
	else if (streq(label, "ood-amount"))
		z->ood_monster_amount = value;
	else if (streq(label, "group-max"))
		z->monster_group_max = value;
	else if (streq(label, "group-dist"))
		z->monster_group_dist = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

/**
 * ZangbandTK (BAL-13, BAL-14): the lethality scalars.
 *
 * Zangband's monsters carry roughly three quarters of vanilla's hit points and
 * half its armour class.  Rather than importing Zangband's per-monster numbers
 * — which would discard twenty-five years of relative tuning — we scale 4.2's
 * own values by the measured deltas.  That keeps 4.2's balance *between*
 * monsters while adopting Zangband's absolute lethality.
 *
 * These live in constants.txt because they are the project's primary balance
 * dial and will be retuned during playtest; keeping them in data means that
 * costs no rebuild.  Values are percentages of the base.
 */
static enum parser_error parse_constants_lethality(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	/*
	 * Reject out of range rather than truncating into the uint16_t field.  The
	 * upper bound also keeps `base * percent` inside an int for the largest
	 * hit point total in the game.
	 */
	if (value <= 0 || value > LETHALITY_MAX)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "hit-points"))
		z->lethality_hp = value;
	else if (streq(label, "armor-class"))
		z->lethality_ac = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

/**
 * ZangbandTK: how pets behave at a level change (PLR-26).
 */
static enum parser_error parse_constants_pets(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0 || value > 255)
		return PARSE_ERROR_INVALID_VALUE;
	if (streq(label, "leave-chance") && value > 100)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-carried"))
		z->pet_max_carried = value;
	else if (streq(label, "carry-radius"))
		z->pet_carry_radius = value;
	else if (streq(label, "leave-chance"))
		z->pet_leave_chance = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

/**
 * ZangbandTK: tuning for the Zangband melee weapon mechanics.
 *
 * Kept in data for the same reason as the lethality scalars — these are
 * frequencies that only playtest can settle, and a rebuild to try a different
 * vorpal rate is a rebuild wasted.
 */
/**
 * ZangbandTK: the wilderness dimensions (WLD-02).
 *
 * In data because the world's size is the project's largest performance and
 * generation risk, and being able to shrink it for testing without a rebuild is
 * worth more than fidelity to any particular number.
 */
static enum parser_error parse_constants_wild(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	/*
	 * Bounded generously and on its own terms.  LETHALITY_MAX is a percentage
	 * ceiling for the balance dials and means nothing here; the monster rarity
	 * dials are odds against, and run into the tens of thousands.
	 */
	if (value <= 0 || value > WILD_VALUE_MAX)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "blocks")) {
		/*
		 * The plasma fractal subdivides by halves and must land on a sample
		 * point at each step, so the map has to be 2^n + 1. Rejecting here
		 * rather than at generation gives the error a file and line.
		 */
		int n = value - 1;

		if (value < 3 || (n & (n - 1)) != 0)
			return PARSE_ERROR_INVALID_VALUE;
		z->wild_blocks = value;
	} else if (streq(label, "block-size")) {
		/*
		 * Blocks are halved to find their centre and their contents hashed by
		 * position within them, so anything under a few grids gives nonsense
		 * rather than a small world.
		 */
		if (value < 4 || value > 256)
			return PARSE_ERROR_INVALID_VALUE;
		z->wild_block_size = value;
	} else if (streq(label, "cache-blocks")) {
		/*
		 * The live window is the largest odd square that fits in this, and it
		 * has to be wider than twice the recentre margin -- otherwise the
		 * margins meet in the middle, every step counts as "near an edge", and
		 * the surface regenerates on each one.  Nine blocks is Zangband's
		 * figure and the smallest that leaves room to walk.
		 */
		if (value < 81)
			return PARSE_ERROR_INVALID_VALUE;
		z->wild_cache_blocks = value;
	} else if (streq(label, "monster-rarity-day")) {
		z->wild_mon_rarity_day = value;
	} else if (streq(label, "monster-rarity-night")) {
		z->wild_mon_rarity_night = value;
	} else if (streq(label, "relic-half-life")) {
		z->relic_half_life = value;
	} else if (streq(label, "quest-slots")) {
		z->quest_slots = value;
	} else if (streq(label, "rivers")) {
		z->wild_rivers = value;
	} else if (streq(label, "lakes")) {
		z->wild_lakes = value;
	} else if (streq(label, "road-dist")) {
		z->wild_road_dist = value;
	} else if (streq(label, "travel-cost")) {
		z->wild_travel_cost = value;
	} else if (streq(label, "heal-cost")) {
		z->heal_cost = value;
	} else if (streq(label, "ailment-cost")) {
		z->ailment_cost = value;
	} else if (streq(label, "restore-cost")) {
		z->restore_cost = value;
	} else if (streq(label, "inn-cost")) {
		z->inn_cost = value;
	} else if (streq(label, "enchant-cost")) {
		z->enchant_cost = value;
	} else if (streq(label, "chaostower-cost")) {
		z->chaostower_cost = value;
	} else if (streq(label, "recharge-cost")) {
		z->recharge_cost = value;
	} else if (streq(label, "gate-turns")) {
		z->wild_gate_turns = value;
	} else if (streq(label, "towns")) {
		z->wild_towns = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_melee(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value <= 0 || value > LETHALITY_MAX)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "vorpal-chance"))
		z->vorpal_chance = value;
	else if (streq(label, "vorpal-multiplier"))
		z->vorpal_multiplier = value;
	else if (streq(label, "chaotic-chance"))
		z->chaotic_chance = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_mon_play(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "break-glyph"))
		z->glyph_hardness = value;
	else if (streq(label, "mult-rate"))
		z->repro_monster_rate = value;
	else if (streq(label, "life-drain"))
		z->life_drain_percent = value;
	else if (streq(label, "flee-range"))
		z->flee_range = value;
	else if (streq(label, "turn-range"))
		z->turn_range = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_dun_gen(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "cent-max"))
		z->level_room_max = value;
	else if (streq(label, "door-max"))
		z->level_door_max = value;
	else if (streq(label, "wall-max"))
		z->wall_pierce_max = value;
	else if (streq(label, "tunn-max"))
		z->tunn_grid_max = value;
	else if (streq(label, "amt-room"))
		z->room_item_av = value;
	else if (streq(label, "amt-item"))
		z->both_item_av = value;
	else if (streq(label, "amt-gold"))
		z->both_gold_av = value;
	else if (streq(label, "pit-max"))
		z->level_pit_max = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_world(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-depth"))
		z->max_depth = value;
	else if (streq(label, "day-length"))
		z->day_length = value;
	else if (streq(label, "blessing-turns"))
		z->blessing_turns = value;
	else if (streq(label, "blessing-bound"))
		z->blessing_bound = value;
	else if (streq(label, "dungeon-hgt"))
		z->dungeon_hgt = value;
	else if (streq(label, "dungeon-wid"))
		z->dungeon_wid = value;
	else if (streq(label, "town-hgt"))
		z->town_hgt = value;
	else if (streq(label, "town-wid"))
		z->town_wid = value;
	else if (streq(label, "feeling-total"))
		z->feeling_total = value;
	else if (streq(label, "feeling-need"))
		z->feeling_need = value;
	else if (streq(label, "stair-skip"))
		z->stair_skip = value;
	else if (streq(label, "move-energy"))
		z->move_energy = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_carry_cap(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "pack-size"))
		z->pack_size = value;
	else if (streq(label, "quiver-size"))
		z->quiver_size = value;
	else if (streq(label, "quiver-slot-size"))
		z->quiver_slot_size = value;
	else if (streq(label, "thrown-quiver-mult"))
		z->thrown_quiver_mult = value;
	else if (streq(label, "floor-size"))
		z->floor_size = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_store(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "inven-max"))
		z->store_inven_max = value;
	else if (streq(label, "turns"))
		z->store_turns = value;
	else if (streq(label, "shuffle"))
		z->store_shuffle = value;
	else if (streq(label, "magic-level"))
		z->store_magic_level = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_obj_make(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-depth"))
		z->max_obj_depth = value;
	else if (streq(label, "great-obj"))
		z->great_obj = value;
	else if (streq(label, "great-ego"))
		z->great_ego = value;
	else if (streq(label, "fuel-torch"))
		z->fuel_torch = value;
	else if (streq(label, "fuel-lamp"))
		z->fuel_lamp = value;
	else if (streq(label, "default-lamp"))
		z->default_lamp = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_player(struct parser *p) {
	struct angband_constants *z;
	const char *label;
	int value;

	z = parser_priv(p);
	label = parser_getsym(p, "label");
	value = parser_getint(p, "value");

	if (value < 0)
		return PARSE_ERROR_INVALID_VALUE;

	if (streq(label, "max-sight"))
		z->max_sight = value;
	else if (streq(label, "max-range"))
		z->max_range = value;
	else if (streq(label, "start-gold"))
		z->start_gold = value;
	else if (streq(label, "food-value"))
		z->food_value = value;
	else
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_melee_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->m_crit_debuff_toh = value;
	} else if (streq(label, "chance-weight-scale")) {
		z->m_crit_chance_weight_scl = value;
	} else if (streq(label, "chance-toh-scale")) {
		z->m_crit_chance_toh_scl = value;
	} else if (streq(label, "chance-level-scale")) {
		z->m_crit_chance_level_scl = value;
	} else if (streq(label, "chance-toh-skill-scale")) {
		z->m_crit_chance_toh_skill_scl = value;
	} else if (streq(label, "chance-offset")) {
		z->m_crit_chance_offset = value;
	} else if (streq(label, "chance-range")) {
		z->m_crit_chance_range = value;
	} else if (streq(label, "power-weight-scale")) {
		z->m_crit_power_weight_scl = value;
	} else if (streq(label, "power-random")) {
		z->m_crit_power_random = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_melee_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct critical_level *new_level;
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->cutoff = parser_getint(p, "cutoff");
	new_level->mult = parser_getint(p, "mult");
	new_level->add = parser_getint(p, "add");
	new_level->msgt = msgt;
	/* Add it to the end of the linked list. */
	if (z->m_crit_level_head) {
		struct critical_level *cursor = z->m_crit_level_head;

		while (cursor->next) {
			cursor = cursor->next;
		}
		cursor->next = new_level;
	} else {
		z->m_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_ranged_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->r_crit_debuff_toh = value;
	} else if (streq(label, "chance-weight-scale")) {
		z->r_crit_chance_weight_scl = value;
	} else if (streq(label, "chance-toh-scale")) {
		z->r_crit_chance_toh_scl = value;
	} else if (streq(label, "chance-level-scale")) {
		z->r_crit_chance_level_scl = value;
	} else if (streq(label, "chance-launched-toh-skill-scale")) {
		z->r_crit_chance_launched_toh_skill_scl = value;
	} else if (streq(label, "chance-thrown-toh-skill-scale")) {
		z->r_crit_chance_thrown_toh_skill_scl = value;
	} else if (streq(label, "chance-offset")) {
		z->r_crit_chance_offset = value;
	} else if (streq(label, "chance-range")) {
		z->r_crit_chance_range = value;
	} else if (streq(label, "power-weight-scale")) {
		z->r_crit_power_weight_scl = value;
	} else if (streq(label, "power-random")) {
		z->r_crit_power_random = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_ranged_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct critical_level *new_level;
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->cutoff = parser_getint(p, "cutoff");
	new_level->mult = parser_getint(p, "mult");
	new_level->add = parser_getint(p, "add");
	new_level->msgt = msgt;
	/* Add it to the end of the linked list. */
	if (z->r_crit_level_head) {
		struct critical_level *cursor = z->r_crit_level_head;

		while (cursor->next) {
			cursor = cursor->next;
		}
		cursor->next = new_level;
	} else {
		z->r_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_melee_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->o_m_crit_debuff_toh = value;
	} else if (streq(label, "power-toh-scale-numerator")) {
		z->o_m_crit_power_toh_scl_num = value;
	} else if (streq(label, "power-toh-scale-denominator")) {
		z->o_m_crit_power_toh_scl_den = value;
	} else if (streq(label, "chance-power-scale-numerator")) {
		z->o_m_crit_chance_power_scl_num = value;
	} else if (streq(label, "chance-power-scale-denominator")) {
		z->o_m_crit_chance_power_scl_den = value;
	} else if (streq(label, "chance-add-denominator")) {
		z->o_m_crit_chance_add_den = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_melee_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct o_critical_level *new_level;
	unsigned int chance = parser_getuint(p, "chance");
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (chance == 0) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->chance = chance;
	new_level->added_dice = parser_getuint(p, "dice");
	new_level->msgt = msgt;
	/* Add it to the end of the linked list. */
	if (z->o_m_crit_level_head) {
		struct o_critical_level *cursor = z->o_m_crit_level_head;

		while (cursor->next) {
			cursor = cursor->next;
		}
		cursor->next = new_level;
	} else {
		z->o_m_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_ranged_critical(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	const char *label = parser_getsym(p, "label");
	int value = parser_getint(p, "value");

	if (streq(label, "debuff-toh")) {
		z->o_r_crit_debuff_toh = value;
	} else if (streq(label, "power-launched-toh-scale-numerator")) {
		z->o_r_crit_power_launched_toh_scl_num = value;
	} else if (streq(label, "power-launched-toh-scale-denominator")) {
		z->o_r_crit_power_launched_toh_scl_den = value;
	} else if (streq(label, "power-thrown-toh-scale-numerator")) {
		z->o_r_crit_power_thrown_toh_scl_num = value;
	} else if (streq(label, "power-thrown-toh-scale-denominator")) {
		z->o_r_crit_power_thrown_toh_scl_den = value;
	} else if (streq(label, "chance-power-scale-numerator")) {
		z->o_r_crit_chance_power_scl_num = value;
	} else if (streq(label, "chance-power-scale-denominator")) {
		z->o_r_crit_chance_power_scl_den = value;
	} else if (streq(label, "chance-add-denominator")) {
		z->o_r_crit_chance_add_den = value;
	} else {
		return PARSE_ERROR_UNDEFINED_DIRECTIVE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_constants_o_ranged_critical_level(struct parser *p)
{
	struct angband_constants *z = parser_priv(p);
	struct o_critical_level *new_level;
	unsigned int chance = parser_getuint(p, "chance");
	const char *msgt_str = parser_getstr(p, "msg");
	int msgt = message_lookup_by_name(msgt_str);

	if (chance == 0) {
		return PARSE_ERROR_INVALID_VALUE;
	}
	if (msgt < 0) {
		return PARSE_ERROR_INVALID_MESSAGE;
	}
	new_level = mem_alloc(sizeof(*new_level));
	new_level->next = NULL;
	new_level->chance = chance;
	new_level->added_dice = parser_getuint(p, "dice");
	new_level->msgt = msgt;
	/* Add it to the end of the linked list. */
	if (z->o_r_crit_level_head) {
		struct o_critical_level *cursor = z->o_r_crit_level_head;

		while (cursor->next) {
			cursor = cursor->next;
		}
		cursor->next = new_level;
	} else {
		z->o_r_crit_level_head = new_level;
	}

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_constants(void) {
	struct angband_constants *z = mem_zalloc(sizeof *z);
	struct parser *p = parser_new();

	parser_setpriv(p, z);
	parser_reg(p, "level-max sym label int value", parse_constants_level_max);
	parser_reg(p, "mon-gen sym label int value", parse_constants_mon_gen);
	parser_reg(p, "lethality sym label int value", parse_constants_lethality);
	parser_reg(p, "pets sym label int value", parse_constants_pets);
	parser_reg(p, "melee sym label int value", parse_constants_melee);
	parser_reg(p, "wild sym label int value", parse_constants_wild);
	parser_reg(p, "mon-play sym label int value", parse_constants_mon_play);
	parser_reg(p, "dun-gen sym label int value", parse_constants_dun_gen);
	parser_reg(p, "world sym label int value", parse_constants_world);
	parser_reg(p, "carry-cap sym label int value", parse_constants_carry_cap);
	parser_reg(p, "store sym label int value", parse_constants_store);
	parser_reg(p, "obj-make sym label int value", parse_constants_obj_make);
	parser_reg(p, "player sym label int value", parse_constants_player);
	parser_reg(p, "melee-critical sym label int value",
		parse_constants_melee_critical);
	parser_reg(p, "melee-critical-level int cutoff int mult int add "
		"str msg", parse_constants_melee_critical_level);
	parser_reg(p, "ranged-critical sym label int value",
		parse_constants_ranged_critical);
	parser_reg(p, "ranged-critical-level int cutoff int mult int add "
		"str msg", parse_constants_ranged_critical_level);
	parser_reg(p, "o-melee-critical sym label int value",
		parse_constants_o_melee_critical);
	parser_reg(p, "o-melee-critical-level uint chance uint dice str msg",
		parse_constants_o_melee_critical_level);
	parser_reg(p, "o-ranged-critical sym label int value",
		parse_constants_o_ranged_critical);
	parser_reg(p, "o-ranged-critical-level uint chance uint dice str msg",
		parse_constants_o_ranged_critical_level);
	return p;
}

static errr run_parse_constants(struct parser *p) {
	return parse_file_quit_not_found(p, "constants");
}

static int check_critical_levels(const struct critical_level *head)
{
	/*
	 * Reject if the cutoffs, except for the last one which is unused, do
	 * not strictly increase.
	 */
	if (!head) {
		return 0;
	}
	while (head->next) {
		int prev_cutoff = head->cutoff;

		head = head->next;
		if (head->next && head->cutoff <= prev_cutoff) {
			return 1;
		}
	}
	return 0;
}

static errr finish_parse_constants(struct parser *p) {
	errr result = PARSE_ERROR_NONE;

	z_info = parser_priv(p);
	parser_destroy(p);

	/*
	 * ZangbandTK: supply defaults for any of our constants the file omitted.
	 *
	 * z_info is zero-allocated and nothing checks these for completeness, so a
	 * constants.txt without the ZangbandTK block — an older installed data
	 * directory, a stale copy, a package shipping vanilla data — would leave
	 * them all zero.  That is not a harmless default: mon_scale_lethality()
	 * reads 0 as "0 percent" and reduces every monster in the game to one hit
	 * point, and one_in_(0) is true on every call, so a vorpal weapon would
	 * double every blow and a chaotic one discharge on every hit.
	 *
	 * The safe absent value for the lethality scalars is 100, which is exactly
	 * vanilla Angband behaviour.
	 */
	if (!z_info->lethality_hp)
		z_info->lethality_hp = 100;
	if (!z_info->lethality_ac)
		z_info->lethality_ac = 100;
	if (!z_info->vorpal_chance)
		z_info->vorpal_chance = 6;
	if (!z_info->vorpal_multiplier)
		z_info->vorpal_multiplier = 2;
	if (!z_info->chaotic_chance)
		z_info->chaotic_chance = 7;
	if (!z_info->wild_blocks)
		z_info->wild_blocks = 33;
	if (!z_info->wild_block_size)
		z_info->wild_block_size = 16;
	if (!z_info->wild_cache_blocks)
		z_info->wild_cache_blocks = 81;
	if (!z_info->wild_mon_rarity_day)
		z_info->wild_mon_rarity_day = 16000;
	if (!z_info->wild_mon_rarity_night)
		z_info->wild_mon_rarity_night = 10000;
	if (!z_info->relic_half_life)
		z_info->relic_half_life = 3;
	if (!z_info->wild_rivers)
		z_info->wild_rivers = 4;
	if (!z_info->wild_lakes)
		z_info->wild_lakes = 4;
	if (!z_info->wild_road_dist)
		z_info->wild_road_dist = 30;
	if (!z_info->wild_travel_cost)
		z_info->wild_travel_cost = 15;
	if (!z_info->heal_cost) z_info->heal_cost = 4;
	if (!z_info->ailment_cost) z_info->ailment_cost = 60;
	if (!z_info->restore_cost) z_info->restore_cost = 400;
	if (!z_info->inn_cost) z_info->inn_cost = 25;
	if (!z_info->enchant_cost) z_info->enchant_cost = 250;
	if (!z_info->recharge_cost) z_info->recharge_cost = 120;
	if (!z_info->wild_gate_turns)
		z_info->wild_gate_turns = 100;
	if (!z_info->wild_towns)
		z_info->wild_towns = 12;
	if (check_critical_levels(z_info->m_crit_level_head)) {
		plog("The cutoffs for melee criticals in constants.txt are "
			"not strictly increasing.");
		if (result == PARSE_ERROR_NONE) {
			result = PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
		}
	}
	if (check_critical_levels(z_info->r_crit_level_head)) {
		plog("The cutoffs for ranged criticals in constants.txt are "
			"not strictly increasing.");
		if (result == PARSE_ERROR_NONE) {
			result = PARSE_ERROR_NON_SEQUENTIAL_RECORDS;
		}
	}
	return result;
}

static void cleanup_critical_levels(struct critical_level *head)
{
	while (head) {
		struct critical_level *target = head;

		head = head->next;
		mem_free(target);
	}
}

static void cleanup_o_critical_levels(struct o_critical_level *head)
{
	while (head) {
		struct o_critical_level *target = head;

		head = head->next;
		mem_free(target);
	}
}

static void cleanup_constants(void)
{
	cleanup_critical_levels(z_info->m_crit_level_head);
	cleanup_critical_levels(z_info->r_crit_level_head);
	cleanup_o_critical_levels(z_info->o_m_crit_level_head);
	cleanup_o_critical_levels(z_info->o_r_crit_level_head);
	mem_free(z_info);
}

struct file_parser constants_parser = {
	"constants",
	init_parse_constants,
	run_parse_constants,
	finish_parse_constants,
	cleanup_constants
};

/**
 * Initialize game constants.
 *
 * Assumption: Paths are set up correctly before calling this function.
 */
void init_game_constants(void)
{
	event_signal_message(EVENT_INITSTATUS, 0, "Initializing constants");
	if (run_parser(&constants_parser))
		quit_fmt("Cannot initialize constants.");
}

/**
 * Free the game constants
 */
static void cleanup_game_constants(void)
{
	cleanup_parser(&constants_parser);
}

/**
 * ------------------------------------------------------------------------
 * Initialize the shop quality ladder (WLD-16a)
 * ------------------------------------------------------------------------ */
struct quality_tier *quality_tiers = NULL;
int quality_tier_count = 0;

static enum parser_error parse_quality_name(struct parser *p) {
	quality_tiers = mem_realloc(quality_tiers,
								(quality_tier_count + 1) * sizeof(*quality_tiers));
	quality_tiers[quality_tier_count].name =
		string_make(parser_getstr(p, "name"));
	quality_tiers[quality_tier_count].level = 0;
	quality_tiers[quality_tier_count].stock = 0;
	++quality_tier_count;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_quality_level(struct parser *p) {
	if (!quality_tier_count) return PARSE_ERROR_MISSING_RECORD_HEADER;

	quality_tiers[quality_tier_count - 1].level = parser_getint(p, "level");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_quality_stock(struct parser *p) {
	if (!quality_tier_count) return PARSE_ERROR_MISSING_RECORD_HEADER;

	quality_tiers[quality_tier_count - 1].stock = parser_getint(p, "stock");

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_quality(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_quality_name);
	parser_reg(p, "level int level", parse_quality_level);
	parser_reg(p, "stock int stock", parse_quality_stock);
	return p;
}

static errr run_parse_quality(struct parser *p) {
	return parse_file_quit_not_found(p, "quality");
}

static errr finish_parse_quality(struct parser *p) {
	int i;

	parser_destroy(p);

	/*
	 * The ladder has to climb.  A rung that stocks no better than the one
	 * below it is a name with nothing behind it, which is the fault WLD-16a
	 * exists to avoid -- Zangband hand-authored 73 of those.
	 */
	for (i = 1; i < quality_tier_count; i++) {
		if (quality_tiers[i].level <= quality_tiers[i - 1].level) {
			plog_fmt("Quality tier '%s' is no better than '%s'",
					 quality_tiers[i].name, quality_tiers[i - 1].name);
			return PARSE_ERROR_INVALID_VALUE;
		}
	}

	return PARSE_ERROR_NONE;
}

static void cleanup_quality(void) {
	int i;

	for (i = 0; i < quality_tier_count; i++)
		string_free(quality_tiers[i].name);

	mem_free(quality_tiers);
	quality_tiers = NULL;
	quality_tier_count = 0;
}

struct file_parser quality_parser = {
	"quality",
	init_parse_quality,
	run_parse_quality,
	finish_parse_quality,
	cleanup_quality
};

/**
 * ------------------------------------------------------------------------
 * Initialize town names (WLD-11)
 * ------------------------------------------------------------------------ */
struct town_names town_names = { NULL, 0, NULL, 0 };

static enum parser_error parse_town_name(struct parser *p) {
	const char *which = parser_getsym(p, "which");
	const char *name = parser_getstr(p, "name");
	char ***list;
	int *count;

	if (streq(which, "settled")) {
		list = &town_names.settled;
		count = &town_names.settled_count;
	} else if (streq(which, "lawless")) {
		list = &town_names.lawless;
		count = &town_names.lawless_count;
	} else {
		return PARSE_ERROR_INVALID_VALUE;
	}

	*list = mem_realloc(*list, (*count + 1) * sizeof(**list));
	(*list)[*count] = string_make(name);
	++*count;

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_town(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "name sym which str name", parse_town_name);
	return p;
}

static errr run_parse_town(struct parser *p) {
	return parse_file_quit_not_found(p, "town");
}

static errr finish_parse_town(struct parser *p) {
	parser_destroy(p);

	/*
	 * A world may hold as many towns as wild:towns asks for, and each wants a
	 * name of its own, so there has to be at least that many to draw from --
	 * otherwise two towns share a name and the player cannot tell which one a
	 * quest or a road is talking about.
	 */
	if (town_names.settled_count + town_names.lawless_count <
			(int) z_info->wild_towns) {
		plog_fmt("Only %d town names for up to %d towns",
			town_names.settled_count + town_names.lawless_count,
			(int) z_info->wild_towns);
		return PARSE_ERROR_TOO_FEW_ENTRIES;
	}

	if (!town_names.settled_count || !town_names.lawless_count) {
		plog("Town names are needed for both settled and lawless country");
		return PARSE_ERROR_MISSING_FIELD;
	}

	return PARSE_ERROR_NONE;
}

static void cleanup_town(void) {
	int i;

	for (i = 0; i < town_names.settled_count; i++)
		string_free(town_names.settled[i]);
	for (i = 0; i < town_names.lawless_count; i++)
		string_free(town_names.lawless[i]);

	mem_free(town_names.settled);
	mem_free(town_names.lawless);

	town_names.settled = NULL;
	town_names.lawless = NULL;
	town_names.settled_count = 0;
	town_names.lawless_count = 0;
}

struct file_parser town_parser = {
	"town",
	init_parse_town,
	run_parse_town,
	finish_parse_town,
	cleanup_town
};

/**
 * ------------------------------------------------------------------------
 * Initialize dungeons (WLD-14)
 * ------------------------------------------------------------------------ */
struct dun_type *dun_types = NULL;

static enum parser_error parse_dungeon_name(struct parser *p) {
	struct dun_type *last = parser_priv(p);
	struct dun_type *d = mem_zalloc(sizeof *d);

	d->name = string_make(parser_getstr(p, "name"));
	d->floor = FEAT_NONE;
	d->rarity = 1;

	if (last) {
		d->index = last->index + 1;
		last->next = d;
	} else {
		d->index = 0;
		dun_types = d;
	}

	if (d->index >= DUN_TYPE_MAX)
		return PARSE_ERROR_TOO_MANY_ENTRIES;

	parser_setpriv(p, d);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_dungeon_depth(struct parser *p) {
	struct dun_type *d = parser_priv(p);
	int min = parser_getint(p, "min"), max = parser_getint(p, "max");

	if (!d) return PARSE_ERROR_MISSING_RECORD_HEADER;

	/*
	 * A dungeon must have at least one level, must start below the surface --
	 * depth zero is the world, not a dungeon -- and must end inside the range
	 * the game knows how to generate.
	 */
	if (min < 1 || max < min || max >= z_info->max_depth)
		return PARSE_ERROR_INVALID_VALUE;

	d->min_depth = min;
	d->max_depth = max;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_dungeon_place(struct parser *p) {
	struct dun_type *d = parser_priv(p);
	int rarity = parser_getint(p, "rarity");

	if (!d) return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (rarity < 1) return PARSE_ERROR_INVALID_VALUE;

	d->rarity = rarity;
	d->pop = parser_getint(p, "pop");
	d->height = parser_getint(p, "height");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_dungeon_theme(struct parser *p) {
	struct dun_type *d = parser_priv(p);
	int treasure = parser_getint(p, "treasure");
	int combat = parser_getint(p, "combat");
	int magic = parser_getint(p, "magic");
	int tools = parser_getint(p, "tools");

	if (!d) return PARSE_ERROR_MISSING_RECORD_HEADER;

	if (treasure < 0 || treasure > 100 || combat < 0 || combat > 100 ||
		magic < 0 || magic > 100 || tools < 0 || tools > 100)
		return PARSE_ERROR_INVALID_VALUE;

	/*
	 * All four at zero would be a dungeon that yields nothing at all, which is
	 * a data mistake rather than a design choice worth allowing.
	 */
	if (!(treasure + combat + magic + tools))
		return PARSE_ERROR_INVALID_VALUE;

	d->theme.treasure = treasure;
	d->theme.combat = combat;
	d->theme.magic = magic;
	d->theme.tools = tools;
	d->has_theme = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_dungeon_dweller(struct parser *p) {
	struct dun_type *d = parser_priv(p);
	const char *what = parser_getsym(p, "what");
	const char *name = parser_getstr(p, "name");

	if (!d) return PARSE_ERROR_MISSING_RECORD_HEADER;

	if (streq(what, "base")) {
		struct monster_base *base = lookup_monster_base(name);

		if (!base) return PARSE_ERROR_INVALID_MONSTER_BASE;
		if (d->dweller_count >= DUN_DWELLERS_MAX)
			return PARSE_ERROR_TOO_MANY_ENTRIES;

		d->dwellers[d->dweller_count++] = base;
	} else if (streq(what, "flag")) {
		if (grab_flag(d->dweller_flags, RF_SIZE, r_info_flags, name))
			return PARSE_ERROR_INVALID_FLAG;
	} else {
		return PARSE_ERROR_INVALID_VALUE;
	}

	d->has_dwellers = true;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_dungeon_floor(struct parser *p) {
	struct dun_type *d = parser_priv(p);
	int feat;

	if (!d) return PARSE_ERROR_MISSING_RECORD_HEADER;

	feat = lookup_feat(parser_getstr(p, "floor"));
	if (feat < 0) return PARSE_ERROR_INVALID_VALUE;

	/*
	 * Checked here rather than trusted: a floor without the FLOOR flag takes
	 * no objects and no staircase, so a dungeon floored with it would generate
	 * levels that could not be left.
	 */
	if (!tf_has(f_info[feat].flags, TF_FLOOR))
		return PARSE_ERROR_INVALID_VALUE;

	d->floor = feat;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_dungeon_profile(struct parser *p) {
	struct dun_type *d = parser_priv(p);

	if (!d) return PARSE_ERROR_MISSING_RECORD_HEADER;

	/*
	 * Not resolved to a profile here: the cave profiles are parsed after this
	 * file, so the name is kept and looked up when a level is generated.
	 */
	string_free(d->profile);
	d->profile = string_make(parser_getstr(p, "profile"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_dungeon_desc(struct parser *p) {
	struct dun_type *d = parser_priv(p);

	if (!d) return PARSE_ERROR_MISSING_RECORD_HEADER;

	d->desc = string_append(d->desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_dungeon(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_dungeon_name);
	parser_reg(p, "depth int min int max", parse_dungeon_depth);
	parser_reg(p, "place int rarity int pop int height", parse_dungeon_place);
	parser_reg(p, "theme int treasure int combat int magic int tools",
			   parse_dungeon_theme);
	parser_reg(p, "dweller sym what str name", parse_dungeon_dweller);
	parser_reg(p, "floor str floor", parse_dungeon_floor);
	parser_reg(p, "profile str profile", parse_dungeon_profile);
	parser_reg(p, "desc str desc", parse_dungeon_desc);
	return p;
}

static errr run_parse_dungeon(struct parser *p) {
	return parse_file_quit_not_found(p, "dungeon");
}

static errr finish_parse_dungeon(struct parser *p) {
	struct dun_type *d;
	int count = 0;

	parser_destroy(p);

	/*
	 * A world with no dungeon in it is a world with nowhere to go, and the
	 * starting town's staircase would lead nowhere.  Better to say so at
	 * startup than to find out on the stairs.
	 */
	for (d = dun_types; d; d = d->next) {
		count++;

		if (!d->min_depth) {
			plog_fmt("Dungeon '%s' has no depth range", d->name);
			return PARSE_ERROR_MISSING_FIELD;
		}
	}

	if (!count) {
		plog("No dungeons defined");
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}

	return PARSE_ERROR_NONE;
}

static void cleanup_dungeon(void) {
	struct dun_type *d = dun_types;

	while (d) {
		struct dun_type *old = d;

		string_free(d->name);
		string_free(d->desc);
		string_free(d->profile);
		d = d->next;
		mem_free(old);
	}

	dun_types = NULL;
}

struct file_parser dungeon_parser = {
	"dungeon",
	init_parse_dungeon,
	run_parse_dungeon,
	finish_parse_dungeon,
	cleanup_dungeon
};

/**
 * Does this monster belong in this dungeon (CNT-05)?
 *
 * By its base -- a tree, a canine, a major demon -- or by a flag the dungeon
 * claims, so that "everything undead" costs one line rather than a list.
 */
bool dun_type_dwells(const struct dun_type *type, const struct monster_race *race)
{
	int i;

	if (!type || !race || !type->has_dwellers)
		return true;

	if (flag_is_inter(race->flags, type->dweller_flags, RF_SIZE))
		return true;

	for (i = 0; i < type->dweller_count; i++)
		if (race->base == type->dwellers[i])
			return true;

	return false;
}

/** How many dungeons the game data defines. */
int dun_type_count(void)
{
	struct dun_type *d;
	int n = 0;

	for (d = dun_types; d; d = d->next) n++;

	return n;
}

/** The dungeon at this place in the list, or NULL. */
struct dun_type *dun_type_by_index(int idx)
{
	struct dun_type *d;

	for (d = dun_types; d; d = d->next)
		if (d->index == idx) return d;

	return NULL;
}

/**
 * The dungeon of this name, or NULL.
 *
 * The savefile stores names rather than indices, so that adding a dungeon to
 * dungeon.txt does not silently move a character into a different one.
 */
struct dun_type *dun_type_by_name(const char *name)
{
	struct dun_type *d;

	if (!name) return NULL;

	for (d = dun_types; d; d = d->next)
		if (streq(d->name, name)) return d;

	return NULL;
}

/**
 * ------------------------------------------------------------------------
 * Initialize world map
 * ------------------------------------------------------------------------ */
static enum parser_error parse_world_level(struct parser *p) {
	const int depth = parser_getint(p, "depth");
	const char *name = parser_getsym(p, "name");
	const char *up = parser_getsym(p, "up");
	const char *down = parser_getsym(p, "down");
	struct level *last = parser_priv(p);
	struct level *lev = mem_zalloc(sizeof *lev);

	if (last) {
		last->next = lev;
	} else {
		world = lev;
	}
	lev->depth = depth;
	lev->name = string_make(name);
	lev->up = streq(up, "None") ? NULL : string_make(up);
	lev->down = streq(down, "None") ? NULL : string_make(down);
	parser_setpriv(p, lev);
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_world(void) {
	struct parser *p = parser_new();

	parser_reg(p, "level int depth sym name sym up sym down",
			   parse_world_level);
	return p;
}

static errr run_parse_world(struct parser *p) {
	return parse_file_quit_not_found(p, "world");
}

static errr finish_parse_world(struct parser *p) {
	struct level *level_check;
	errr result = PARSE_ERROR_NONE;
	int maxe = get_parser_error_limit(), counte = 0;

	/* Check that all levels referred to exist */
	for (level_check = world; level_check; level_check = level_check->next) {
		struct level *level_find = world;

		/* Check upwards */
		if (level_check->up) {
			while (level_find && !streq(level_check->up, level_find->name)) {
				level_find = level_find->next;
			}
			if (!level_find) {
				if (result == PARSE_ERROR_NONE) {
					result = PARSE_ERROR_INVALID_VALUE;
				}
				plog_fmt("Invalid up level reference, %s, "
					"for level %s", level_check->up,
					level_check->name);
				if (maxe) {
					if (counte >= maxe - 1) {
						break;
					}
					++counte;
				}
			}
		}

		/* Check downwards */
		level_find = world;
		if (level_check->down) {
			while (level_find && !streq(level_check->down, level_find->name)) {
				level_find = level_find->next;
			}
			if (!level_find) {
				if (result == PARSE_ERROR_NONE) {
					result = PARSE_ERROR_INVALID_VALUE;
				}
				plog_fmt("Invalid down level reference, %s, "
					"for level %s", level_check->down,
					level_check->name);
				if (maxe) {
					if (counte >= maxe - 1) {
						break;
					}
					++counte;
				}
			}
		}
	}

	parser_destroy(p);
	return result;
}

static void cleanup_world(void)
{
	struct level *level = world;
	while (level) {
		struct level *old = level;
		string_free(level->name);
		string_free(level->up);
		string_free(level->down);
		level = level->next;
		mem_free(old);
	}
}

struct file_parser world_parser = {
	"world",
	init_parse_world,
	run_parse_world,
	finish_parse_world,
	cleanup_world
};


/**
 * ------------------------------------------------------------------------
 * Initialize player properties
 * ------------------------------------------------------------------------ */
/*
 * Keep track of UI entries to be bound to an ability while parsing.  Bind them
 * at the end of parsing and don't pass them along to the stored player_ability
 * structures.
 */
struct player_bound_ui {
	char *name;
	struct player_bound_ui *next;
	int value;
	bool isaux;
	bool isspecial;
};
struct embryo_player_ability {
	struct player_ability ability;
	struct player_bound_ui *boundui;
	struct embryo_player_ability *next;
};
static struct embryo_player_ability  *embryo_player_abilities = NULL;

static enum parser_error parse_player_prop_type(struct parser *p) {
	const char *type = parser_getstr(p, "type");
	struct embryo_player_ability *h = parser_priv(p);
	struct embryo_player_ability *embryo = mem_zalloc(sizeof *embryo);

	if (h) {
		h->next = embryo;
	} else {
		embryo_player_abilities = embryo;
	}
	parser_setpriv(p, embryo);
	embryo->ability.type = string_make(type);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	struct embryo_player_ability *embryo = parser_priv(p);
	int index = -1;

	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!embryo->ability.type)
		return PARSE_ERROR_MISSING_PLAY_PROP_TYPE;

	if (streq(embryo->ability.type, "player")) {
		index = code_index_in_array(player_info_flags, code);
	} else if (streq(embryo->ability.type, "object")) {
		index = code_index_in_array(list_obj_flag_names, code);
	}
	if (index >= 0) {
		embryo->ability.index = index;
	} else {
		return PARSE_ERROR_INVALID_PLAY_PROP_CODE;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_desc(struct parser *p) {
	struct embryo_player_ability *embryo = parser_priv(p);
	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	embryo->ability.desc = string_append(embryo->ability.desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_name(struct parser *p) {
	const char *desc = parser_getstr(p, "desc");
	struct embryo_player_ability *embryo = parser_priv(p);

	if (!embryo) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(embryo->ability.name);
	embryo->ability.name = string_make(desc);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_value(struct parser *p) {
	struct embryo_player_ability *embryo = parser_priv(p);
	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	embryo->ability.value = parser_getint(p, "value");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_player_prop_bindui(struct parser *p) {
	const char *name = parser_getsym(p, "ui");
	const char *value = parser_getsym(p, "uival");
	bool isaux = (parser_getint(p, "aux") != 0);
	struct embryo_player_ability *embryo = parser_priv(p);
	struct player_bound_ui *boundui;
	if (!embryo)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	boundui = mem_alloc(sizeof(*boundui));
	boundui->name = string_make(name);
	if (streq(value, "special")) {
		boundui->value = 0;
		boundui->isspecial = true;
	} else {
		long v;
		char* end;

		v = strtol(value, &end, 10);
		if (! *value || *end) {
			string_free(boundui->name);
			mem_free(boundui);
			return PARSE_ERROR_NOT_NUMBER;
		}
		/*
		 * Also reject INT_MIN and INT_MAX so we don't have to check
		 * errno to detect out of range values on platforms where
		 * sizeof(int) == sizeof(long).
		 */
		if (v <= INT_MIN || v >= INT_MAX) {
			string_free(boundui->name);
			mem_free(boundui);
			return PARSE_ERROR_INVALID_VALUE;
		}
		boundui->value = (int) v;
		boundui->isspecial = false;
	}
	boundui->isaux = isaux;
	boundui->next = embryo->boundui;
	embryo->boundui = boundui;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_player_prop(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "type str type", parse_player_prop_type);
	parser_reg(p, "code str code", parse_player_prop_code);
	parser_reg(p, "desc str desc", parse_player_prop_desc);
	parser_reg(p, "name str desc", parse_player_prop_name);
	parser_reg(p, "value int value", parse_player_prop_value);
	parser_reg(p, "bindui sym ui int aux sym uival", parse_player_prop_bindui);
	return p;
}

static errr run_parse_player_prop(struct parser *p) {
	return parse_file_quit_not_found(p, "player_property");
}

static errr finish_parse_player_prop(struct parser *p) {
	struct embryo_player_ability *embryo = embryo_player_abilities;
	struct embryo_player_ability *target;
	struct player_bound_ui *boundui_cursor;
	struct player_ability *new, *previous = NULL;

	embryo_player_abilities = NULL;
	/* Copy abilities over, making multiple copies for element types */
	player_abilities = mem_zalloc(sizeof(*player_abilities));
	new = player_abilities;
	while (embryo) {
		if (streq(embryo->ability.type, "element")) {
			uint16_t i, n;
			assert(N_ELEMENTS(list_element_names) < 65536);
			n = (uint16_t) N_ELEMENTS(list_element_names);
			for (i = 0; i < n - 1; i++) {
				char *name = string_make(projections[i].name);
				new->index = i;
				new->type = string_make(embryo->ability.type);
				new->desc = string_make(format("%s %s.", embryo->ability.desc, name));
				my_strcap(name);
				new->name = string_make(format("%s %s", name, embryo->ability.name));
				string_free(name);
				new->value = embryo->ability.value;
				boundui_cursor = embryo->boundui;
				while (boundui_cursor) {
					name = string_make(format("%s<%s>", boundui_cursor->name, list_element_names[i]));
					(void) bind_player_ability_to_ui_entry_by_name(name, new, boundui_cursor->value, !boundui_cursor->isspecial, boundui_cursor->isaux);
					string_free(name);
					boundui_cursor = boundui_cursor->next;
				}
				if ((i != n - 2) || embryo->next) {
					previous = new;
					new = mem_zalloc(sizeof(*new));
					previous->next = new;
				}
			}
			string_free(embryo->ability.type);
			string_free(embryo->ability.desc);
			string_free(embryo->ability.name);
			while (embryo->boundui) {
				boundui_cursor = embryo->boundui;
				embryo->boundui = embryo->boundui->next;
				string_free(boundui_cursor->name);
				mem_free(boundui_cursor);
			}
		} else {
			new->type = embryo->ability.type;
			new->index = embryo->ability.index;
			new->desc = embryo->ability.desc;
			new->name = embryo->ability.name;
			while (embryo->boundui) {
				boundui_cursor = embryo->boundui;
				embryo->boundui = embryo->boundui->next;
				(void) bind_player_ability_to_ui_entry_by_name(boundui_cursor->name, new, boundui_cursor->value, !boundui_cursor->isspecial, boundui_cursor->isaux);
				string_free(boundui_cursor->name);
				mem_free(boundui_cursor);
			}
			if (embryo->next) {
				previous = new;
				new = mem_zalloc(sizeof(*new));
				previous->next = new;
			}
		}
		target = embryo;
		embryo = embryo->next;
		mem_free(target);
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_player_prop(void)
{
	struct player_ability *ability = player_abilities;
	while (ability) {
		struct player_ability *totrash = ability;
		ability = ability->next;
		string_free(totrash->type);
		string_free(totrash->desc);
		string_free(totrash->name);
		mem_free(totrash);
	}
}

struct file_parser player_property_parser = {
	"player_property",
	init_parse_player_prop,
	run_parse_player_prop,
	finish_parse_player_prop,
	cleanup_player_prop
};

/**
 * ------------------------------------------------------------------------
 * Initialize random names
 * ------------------------------------------------------------------------ */

struct name {
	struct name *next;
	char *str;
};

struct names_parse {
	unsigned int section;
	unsigned int nnames[RANDNAME_NUM_TYPES];
	struct name *names[RANDNAME_NUM_TYPES];
};

static enum parser_error parse_names_section(struct parser *p) {
	unsigned int section = parser_getuint(p, "section");
	struct names_parse *s = parser_priv(p);

	if (section >= RANDNAME_NUM_TYPES) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	s->section = section;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_names_word(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct names_parse *s = parser_priv(p);
	struct name *ns = mem_zalloc(sizeof *ns);

	s->nnames[s->section]++;
	ns->next = s->names[s->section];
	ns->str = string_make(name);
	s->names[s->section] = ns;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_names(void) {
	struct parser *p = parser_new();
	struct names_parse *n = mem_zalloc(sizeof *n);
	n->section = 0;
	parser_setpriv(p, n);
	parser_reg(p, "section uint section", parse_names_section);
	parser_reg(p, "word str name", parse_names_word);
	return p;
}

static errr run_parse_names(struct parser *p) {
	return parse_file_quit_not_found(p, "names");
}

static errr finish_parse_names(struct parser *p) {
	int i;
	unsigned int j;
	struct names_parse *n = parser_priv(p);
	struct name *nm;
	name_sections = mem_zalloc(sizeof(char**) * RANDNAME_NUM_TYPES);
	for (i = 0; i < RANDNAME_NUM_TYPES; i++) {
		name_sections[i] = mem_alloc(sizeof(char*) * (n->nnames[i] + 1));
		for (nm = n->names[i], j = 0; nm && j < n->nnames[i]; nm = nm->next, j++) {
			name_sections[i][j] = nm->str;
		}
		name_sections[i][n->nnames[i]] = NULL;
		while (n->names[i]) {
			nm = n->names[i]->next;
			mem_free(n->names[i]);
			n->names[i] = nm;
		}
	}
	mem_free(n);
	parser_destroy(p);
	return 0;
}

static void cleanup_names(void)
{
	int i, j;
	for (i = 0; i < RANDNAME_NUM_TYPES; i++) {
		for (j = 0; name_sections[i][j]; j++) {
			string_free((char *)name_sections[i][j]);
		}
		mem_free((char**)name_sections[i]);
	}
	mem_free((char***)name_sections);
}

struct file_parser names_parser = {
	"names",
	init_parse_names,
	run_parse_names,
	finish_parse_names,
	cleanup_names
};

/**
 * ------------------------------------------------------------------------
 * Initialize traps
 * ------------------------------------------------------------------------ */

static enum parser_error parse_trap_name(struct parser *p) {
    const char *name = parser_getsym(p, "name");
    const char *desc = parser_getstr(p, "desc");
    struct trap_kind *h = parser_priv(p);

    struct trap_kind *t = mem_zalloc(sizeof *t);
    t->next = h;
    t->name = string_make(name);
	t->desc = string_make(desc);
    parser_setpriv(p, t);
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_graphics(struct parser *p) {
    wchar_t glyph = parser_getchar(p, "glyph");
    const char *color = parser_getsym(p, "color");
    int attr = 0;
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->d_char = glyph;
    if (strlen(color) > 1)
		attr = color_text_to_attr(color);
    else
		attr = color_char_to_attr(color[0]);
    if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
    t->d_attr = attr;
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_appear(struct parser *p) {
    struct trap_kind *t = parser_priv(p);

    if (!t)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
    t->rarity =  parser_getuint(p, "rarity");
    t->min_depth =  parser_getuint(p, "mindepth");
    t->max_num =  parser_getuint(p, "maxnum");
    return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_visibility(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	dice_t *dice;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	dice = dice_new();
	if (!dice_parse_string(dice, parser_getstr(p, "visibility"))) {
		dice_free(dice);
		return PARSE_ERROR_NOT_RANDOM;
	}
	dice_random_value(dice, &t->power);
	dice_free(dice);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_flags(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	char *flags, *s;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "flags")) {
		return PARSE_ERROR_NONE;
	}
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(t->flags, TRF_SIZE, trap_flags, s)) {
			break;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_effect(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*new_effect));
	if (t->effect) {
		effect = t->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		t->effect = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_trap_effect_yx(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_dice(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	dice_t *dice;
	const char *string;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_expr(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}
	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_trap_effect_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*new_effect));
	if (t->effect_xtra) {
		effect = t->effect_xtra;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		t->effect_xtra = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_trap_effect_yx_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect_xtra;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_dice_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	dice_t *dice;
	const char *string;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect_xtra;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_expr_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = t->effect_xtra;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}

	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_trap_save_flags(struct parser *p) {
	struct trap_kind *t = parser_priv(p);
	char *s, *u;

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	s = string_make(parser_getstr(p, "flags"));
	u = strtok(s, " |");
	while (u) {
		if (grab_flag(t->save_flags, OF_SIZE, list_obj_flag_names, u)) {
			break;
		}
		u = strtok(NULL, " |");
	}
	string_free(s);
	return u ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_desc(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->text = string_append(t->text, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg = string_append(t->msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_good(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg_good = string_append(t->msg_good, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_bad(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg_bad = string_append(t->msg_bad, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_trap_msg_xtra(struct parser *p) {
	struct trap_kind *t = parser_priv(p);

	if (!t) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	t->msg_xtra = string_append(t->msg_xtra, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_trap(void) {
    struct parser *p = parser_new();
    parser_setpriv(p, NULL);
    parser_reg(p, "name sym name str desc", parse_trap_name);
    parser_reg(p, "graphics char glyph sym color", parse_trap_graphics);
    parser_reg(p, "appear uint rarity uint mindepth uint maxnum", parse_trap_appear);
    parser_reg(p, "visibility str visibility", parse_trap_visibility);
    parser_reg(p, "flags ?str flags", parse_trap_flags);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_trap_effect);
	parser_reg(p, "effect-yx int y int x", parse_trap_effect_yx);
	parser_reg(p, "dice str dice", parse_trap_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_trap_expr);
	parser_reg(p, "effect-xtra sym eff ?sym type ?int radius ?int other", parse_trap_effect_xtra);
	parser_reg(p, "effect-yx-xtra int y int x", parse_trap_effect_yx_xtra);
	parser_reg(p, "dice-xtra str dice", parse_trap_dice_xtra);
	parser_reg(p, "expr-xtra sym name sym base str expr", parse_trap_expr_xtra);
	parser_reg(p, "save str flags", parse_trap_save_flags);
	parser_reg(p, "desc str text", parse_trap_desc);
	parser_reg(p, "msg str text", parse_trap_msg);
	parser_reg(p, "msg-good str text", parse_trap_msg_good);
	parser_reg(p, "msg-bad str text", parse_trap_msg_bad);
	parser_reg(p, "msg-xtra str text", parse_trap_msg_xtra);
    return p;
}

static errr run_parse_trap(struct parser *p) {
    return parse_file_quit_not_found(p, "trap");
}

static errr finish_parse_trap(struct parser *p) {
	struct trap_kind *t, *n;
	int tidx;

	/* Scan the list for the max id */
	z_info->trap_max = 0;
	t = parser_priv(p);
	while (t) {
		z_info->trap_max++;
		t = t->next;
	}

	trap_info = mem_zalloc((z_info->trap_max + 1) * sizeof(*t));
	tidx = z_info->trap_max - 1;
    for (t = parser_priv(p); t; t = t->next, tidx--) {
		assert(tidx >= 0);

		memcpy(&trap_info[tidx], t, sizeof(*t));
		trap_info[tidx].tidx = tidx;
		if (tidx < z_info->trap_max - 1)
			trap_info[tidx].next = &trap_info[tidx + 1];
		else
			trap_info[tidx].next = NULL;
    }

    t = parser_priv(p);
    while (t) {
		n = t->next;
		mem_free(t);
		t = n;
    }

    parser_destroy(p);
    return 0;
}

static void cleanup_trap(void)
{
	int i;
	for (i = 0; i < z_info->trap_max; i++) {
		string_free(trap_info[i].name);
		mem_free(trap_info[i].text);
		string_free(trap_info[i].desc);
		string_free(trap_info[i].msg);
		string_free(trap_info[i].msg_good);
		string_free(trap_info[i].msg_bad);
		string_free(trap_info[i].msg_xtra);
		free_effect(trap_info[i].effect);
		free_effect(trap_info[i].effect_xtra);
	}
	mem_free(trap_info);
}

struct file_parser trap_parser = {
    "trap",
    init_parse_trap,
    run_parse_trap,
    finish_parse_trap,
    cleanup_trap
};

/**
 * ------------------------------------------------------------------------
 * Initialize terrain
 * ------------------------------------------------------------------------ */

static enum parser_error parse_feat_code(struct parser *p) {
	const char *code = parser_getstr(p, "code");
	int idx = lookup_feat_code(code);
	struct feature *f;

	if (idx < 0) {
		/*
		 * Of the existing parser errors, PARSE_ERROR_INVALID_VALUE
		 * could also be used; this matches what ui-prefs.c returns
		 * for an unknown feature code or name.
		 */
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	assert(idx < FEAT_MAX);
	f = &f_info[idx];
	f->fidx = idx;
	parser_setpriv(p, f);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_name(struct parser *p) {
	const char *name = parser_getstr(p, "name");
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (f->name) {
		return PARSE_ERROR_REPEATED_DIRECTIVE;
	}
	f->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_graphics(struct parser *p) {
	wchar_t glyph = parser_getchar(p, "glyph");
	const char *color = parser_getsym(p, "color");
	int attr = 0;
	struct feature *f = parser_priv(p);

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	f->d_char = glyph;
	if (strlen(color) > 1)
		attr = color_text_to_attr(color);
	else
		attr = color_char_to_attr(color[0]);
	if (attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
	f->d_attr = attr;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_mimic(struct parser *p) {
	const char *mimic_name = parser_getstr(p, "feat");
	struct feature *f = parser_priv(p);
	int mimic_idx;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Verify that it refers to a valid feature. */
	mimic_idx = lookup_feat_code(mimic_name);
	if (mimic_idx < 0) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	f->mimic = &f_info[mimic_idx];
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_priority(struct parser *p) {
	unsigned int priority = parser_getuint(p, "priority");
	struct feature *f = parser_priv(p);

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	f->priority = priority;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_flags(struct parser *p) {
	struct feature *f = parser_priv(p);
	char *flags, *s;

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (!parser_hasval(p, "flags")) {
		return PARSE_ERROR_NONE;
	}
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(f->flags, TF_SIZE, terrain_flags, s)) {
			break;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_digging(struct parser *p) {
	struct feature *f = parser_priv(p);
	int dig_idx = parser_getint(p, "dig");

	if (!f)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (dig_idx < DIGGING_RUBBLE + 1 || dig_idx >= DIGGING_MAX + 1) {
		return PARSE_ERROR_OUT_OF_BOUNDS;
	}
	f->dig = dig_idx;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_desc(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->desc = string_append(f->desc, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_walk_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->walk_msg = string_append(f->walk_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_run_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->run_msg = string_append(f->run_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_hurt_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->hurt_msg = string_append(f->hurt_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_die_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->die_msg = string_append(f->die_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_confused_msg(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->confused_msg =
		string_append(f->confused_msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_look_prefix(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->look_prefix =
		string_append(f->look_prefix, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_look_in_preposition(struct parser *p) {
	struct feature *f = parser_priv(p);

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	f->look_in_preposition =
		string_append(f->look_in_preposition, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_feat_resist_flag(struct parser *p) {
	struct feature *f = parser_priv(p);
	int flag = lookup_flag(mon_race_flags, parser_getsym(p, "flag"));

	if (!f) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (flag == FLAG_END) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	f->resist_flag = flag;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_feat(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "code str code", parse_feat_code);
	parser_reg(p, "name str name", parse_feat_name);
	parser_reg(p, "graphics char glyph sym color", parse_feat_graphics);
	parser_reg(p, "mimic str feat", parse_feat_mimic);
	parser_reg(p, "priority uint priority", parse_feat_priority);
	parser_reg(p, "flags ?str flags", parse_feat_flags);
	parser_reg(p, "digging int dig", parse_feat_digging);
	parser_reg(p, "desc str text", parse_feat_desc);
	parser_reg(p, "walk-msg str text", parse_feat_walk_msg);
	parser_reg(p, "run-msg str text", parse_feat_run_msg);
	parser_reg(p, "hurt-msg str text", parse_feat_hurt_msg);
	parser_reg(p, "die-msg str text", parse_feat_die_msg);
	parser_reg(p, "confused-msg str text", parse_feat_confused_msg);
	parser_reg(p, "look-prefix str text", parse_feat_look_prefix);
	parser_reg(p, "look-in-preposition str text", parse_feat_look_in_preposition);
	parser_reg(p, "resist-flag sym flag", parse_feat_resist_flag);

	/*
	 * Since the layout of the terrain array is fixed by list-terrain.h,
	 * allocate it now and fill in the customizable parts when parsing.
	 */
	f_info = mem_zalloc(FEAT_MAX * sizeof(*f_info));

	return p;
}

static errr run_parse_feat(struct parser *p) {
	return parse_file_quit_not_found(p, "terrain");
}

static errr finish_parse_feat(struct parser *p) {
	int shop_idx = 0, fidx;

	for (fidx = 0; fidx < FEAT_MAX; ++fidx) {
		/*
		 * Assign shop index based on the order within the other
		 * terrain.
		 */
		if (tf_has(f_info[fidx].flags, TF_SHOP)) {
			f_info[fidx].shopnum = ++shop_idx;
		}
		/*
		 * Ensure the prefixes and prepositions end with a space for
		 * ease of use with the targeting code.
		 */
		if (f_info[fidx].look_prefix && !suffix(
				f_info[fidx].look_prefix, " ")) {
			f_info[fidx].look_prefix = string_append(
				f_info[fidx].look_prefix, " ");
		}
		if (f_info[fidx].look_in_preposition && !suffix(
				f_info[fidx].look_in_preposition, " ")) {
			f_info[fidx].look_in_preposition =
				string_append(f_info[fidx].look_in_preposition,
				" ");
		}
	}
	z_info->store_max = shop_idx;

	parser_destroy(p);
	return 0;
}

static void cleanup_feat(void) {
	int idx;
	for (idx = 0; idx < FEAT_MAX; idx++) {
		string_free(f_info[idx].look_in_preposition);
		string_free(f_info[idx].look_prefix);
		string_free(f_info[idx].confused_msg);
		string_free(f_info[idx].die_msg);
		string_free(f_info[idx].hurt_msg);
		string_free(f_info[idx].run_msg);
		string_free(f_info[idx].walk_msg);
		string_free(f_info[idx].desc);
		string_free(f_info[idx].name);
	}
	mem_free(f_info);
}

struct file_parser feat_parser = {
	"terrain",
	init_parse_feat,
	run_parse_feat,
	finish_parse_feat,
	cleanup_feat
};

/**
 * ------------------------------------------------------------------------
 * Initialize player bodies
 * ------------------------------------------------------------------------ */

static enum parser_error parse_body_body(struct parser *p) {
	struct player_body *h = parser_priv(p);
	struct player_body *b = mem_zalloc(sizeof *b);

	b->next = h;
	b->name = string_make(parser_getstr(p, "name"));
	parser_setpriv(p, b);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_body_slot(struct parser *p) {
	struct player_body *b = parser_priv(p);
	struct equip_slot *slot;
	int n;

	if (!b) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the last valid slot, then allocate a new one */
	slot = b->slots;
	if (!slot) {
		b->slots = mem_zalloc(sizeof(struct equip_slot));
		slot = b->slots;
	} else {
		while (slot->next) slot = slot->next;
		slot->next = mem_zalloc(sizeof(struct equip_slot));
		slot = slot->next;
	}

	n = lookup_flag(slots, parser_getsym(p, "slot"));
	if (!n) {
		return PARSE_ERROR_INVALID_FLAG;
	}
	slot->type = n;
	slot->name = string_make(parser_getsym(p, "name"));
	b->count++;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_body(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "body str name", parse_body_body);
	parser_reg(p, "slot sym slot sym name", parse_body_slot);
	return p;
}

static errr run_parse_body(struct parser *p) {
	return parse_file_quit_not_found(p, "body");
}

static errr finish_parse_body(struct parser *p) {
	struct player_body *b;
	int i;
	bodies = parser_priv(p);

	/* Scan the list for the max slots */
	z_info->equip_slots_max = 0;
	for (b = bodies; b; b = b->next) {
		if (b->count > z_info->equip_slots_max)
			z_info->equip_slots_max = b->count;
	}

	/* Allocate the slot list and copy */
	for (b = bodies; b; b = b->next) {
		struct equip_slot *s_new;

		s_new = mem_zalloc(z_info->equip_slots_max * sizeof(*s_new));
		if (b->slots) {
			struct equip_slot *s_temp, *s_old = b->slots;

			/* Allocate space and copy */
			for (i = 0; i < z_info->equip_slots_max; i++) {
				memcpy(&s_new[i], s_old, sizeof(*s_old));
				s_old = s_old->next;
				if (!s_old) break;
			}

			/* Make next point correctly */
			for (i = 0; i < z_info->equip_slots_max; i++)
				if (s_new[i].next)
					s_new[i].next = &s_new[i + 1];

			/* Tidy up */
			s_old = b->slots;
			s_temp = s_old;
			while (s_temp) {
				s_temp = s_old->next;
				mem_free(s_old);
				s_old = s_temp;
			}
		}
		b->slots = s_new;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_body(void)
{
	struct player_body *b = bodies;
	struct player_body *next;
	int i;

	while (b) {
		next = b->next;
		string_free((char *)b->name);
		for (i = 0; i < b->count; i++)
			string_free((char *)b->slots[i].name);
		mem_free(b->slots);
		mem_free(b);
		b = next;
	}
}

struct file_parser body_parser = {
	"body",
	init_parse_body,
	run_parse_body,
	finish_parse_body,
	cleanup_body
};

/**
 * ------------------------------------------------------------------------
 * Initialize player histories
 * ------------------------------------------------------------------------ */

static struct history_chart *histories;

static struct history_chart *findchart(struct history_chart *hs,
									   unsigned int idx) {
	for (; hs; hs = hs->next)
		if (hs->idx == idx)
			break;
	return hs;
}

static enum parser_error parse_history_chart(struct parser *p) {
	struct history_chart *oc = parser_priv(p);
	struct history_chart *c;
	struct history_entry *e = mem_zalloc(sizeof *e);
	unsigned int idx = parser_getuint(p, "chart");

	if (!(c = findchart(oc, idx))) {
		c = mem_zalloc(sizeof *c);
		c->next = oc;
		c->idx = idx;
		parser_setpriv(p, c);
	}

	e->isucc = parser_getint(p, "next");
	e->roll = parser_getint(p, "roll");

	e->next = c->entries;
	c->entries = e;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_history_phrase(struct parser *p) {
	struct history_chart *h = parser_priv(p);

	if (!h)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	assert(h->entries);
	h->entries->text = string_append(h->entries->text, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_history(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "chart uint chart int next int roll", parse_history_chart);
	parser_reg(p, "phrase str text", parse_history_phrase);
	return p;
}

static errr run_parse_history(struct parser *p) {
	return parse_file_quit_not_found(p, "history");
}

static errr finish_parse_history(struct parser *p) {
	struct history_chart *c;
	struct history_entry *e, *prev, *next;
	errr result = PARSE_ERROR_NONE;
	int maxe = get_parser_error_limit(), counte = 0;

	histories = parser_priv(p);

	/* Go fix up the entry successor pointers. We can't compute them at
	 * load-time since we may not have seen the successor history yet. Also,
	 * we need to put the entries in the right order; the parser actually
	 * stores them backwards, which is not desirable.
	 */
	for (c = histories; c; c = c->next) {
		e = c->entries;
		prev = NULL;
		while (e) {
			next = e->next;
			e->next = prev;
			prev = e;
			e = next;
		}
		c->entries = prev;
		for (e = c->entries; e; e = e->next) {
			if (!e->isucc)
				continue;
			e->succ = findchart(histories, e->isucc);
			if (!e->succ) {
				if (result == PARSE_ERROR_NONE) {
					result = PARSE_ERROR_INVALID_VALUE;
				}
				plog_fmt("No successor found for history "
					"entry, '%s': requested successor "
					"is %d", e->text, e->isucc);
				if (maxe) {
					if (counte >= maxe - 1) {
						break;
					}
					++counte;
				}
			}
		}
	}

	parser_destroy(p);
	return result;
}

static void cleanup_history(void)
{
	struct history_chart *c, *next_c;
	struct history_entry *e, *next_e;

	c = histories;
	while (c) {
		next_c = c->next;
		e = c->entries;
		while (e) {
			next_e = e->next;
			mem_free(e->text);
			mem_free(e);
			e = next_e;
		}
		mem_free(c);
		c = next_c;
	}
}

struct file_parser history_parser = {
	"history",
	init_parse_history,
	run_parse_history,
	finish_parse_history,
	cleanup_history
};

/**
 * ------------------------------------------------------------------------
 * Initialize player races
 * ------------------------------------------------------------------------ */

/**
 * Parse a `virtues:` line into a fixed array of virtue indices (PLR-19).
 *
 * Shared by class.txt, p_race.txt and realm.txt, which all say the same thing
 * in the same words: an ordered list of virtues, of which the ones not already
 * held are taken. Unknown names are an error rather than a silent zero -- a
 * misspelt virtue would otherwise leave a class quietly measured against one
 * thing fewer.
 */
static enum parser_error grab_virtues(struct parser *p, int *out, int max)
{
	char *flags, *s;
	int n = 0;

	if (!parser_hasval(p, "virtues")) return PARSE_ERROR_NONE;

	flags = string_make(parser_getstr(p, "virtues"));
	s = strtok(flags, " |");
	while (s) {
		int i;
		bool found = false;

		for (i = 1; i < V_MAX; i++) {
			if (my_stricmp(s, virtue_code(i))) continue;
			if (n < max) out[n++] = i;
			found = true;
			break;
		}
		if (!found) {
			string_free(flags);
			return PARSE_ERROR_INVALID_VALUE;
		}
		s = strtok(NULL, " |");
	}
	string_free(flags);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_virtues(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return grab_virtues(p, c->virtues, MAX_CLASS_VIRTUES);
}

/*
 * A mutation this race tends towards, and how strongly (PLR-38).
 *
 * `mutation-affinity:HYPN_GAZE:7` is a Vampire: when a mutation is rolled,
 * six times in ten it is that one instead. The numbers differ per race and
 * the spoiler does not say so -- a Beastman takes polymorph self only one
 * time in ten ([mutation.c:552](../archive/zangband/src/mutation.c#L552)).
 */
static enum parser_error parse_p_race_mutation(struct parser *p) {
	struct player_race *r = parser_priv(p);

	if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

	r->mutation_affinity = string_make(parser_getsym(p, "code"));
	r->mutation_chance = parser_getint(p, "chance");

	return PARSE_ERROR_NONE;
}

/*
 * How readily this race mutates on its own (PLR-36).
 *
 * `mutation-rate:1:20` is a Beastman: one mutation at character creation and
 * a 20% chance at every level after. It is the only race in Zangband that
 * mutates without being made to, and the spoiler leads with it.
 */
static enum parser_error parse_p_race_mutation_rate(struct parser *p) {
	struct player_race *r = parser_priv(p);

	if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

	r->mutation_birth = parser_getint(p, "birth");
	r->mutation_per_level = parser_getint(p, "level");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_virtues(struct parser *p) {
	struct player_race *r = parser_priv(p);

	if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return grab_virtues(p, r->virtues, MAX_RACE_VIRTUES);
}

static enum parser_error parse_realm_virtues(struct parser *p) {
	struct magic_realm *realm = parser_priv(p);

	if (!realm) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return grab_virtues(p, realm->virtues, MAX_REALM_VIRTUES);
}

static enum parser_error parse_p_race_name(struct parser *p) {
	struct player_race *h = parser_priv(p);
	struct player_race *r = mem_zalloc(sizeof *r);

	r->next = h;
	r->name = string_make(parser_getstr(p, "name"));
	/* Default body is humanoid */
	r->body = 0;
	parser_setpriv(p, r);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_stats(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_adj[STAT_STR] = parser_getint(p, "str");
	r->r_adj[STAT_DEX] = parser_getint(p, "dex");
	r->r_adj[STAT_CON] = parser_getint(p, "con");
	r->r_adj[STAT_INT] = parser_getint(p, "int");
	r->r_adj[STAT_WIS] = parser_getint(p, "wis");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_disarm_phys(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_DISARM_PHYS] = parser_getint(p, "disarm");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_disarm_magic(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "disarm");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_device(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_DEVICE] = parser_getint(p, "device");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_save(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_SAVE] = parser_getint(p, "save");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_stealth(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_STEALTH] = parser_getint(p, "stealth");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_search(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_SEARCH] = parser_getint(p, "search");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_melee(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "melee");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_shoot(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "shoot");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_throw(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "throw");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_skill_dig(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_skills[SKILL_DIGGING] = parser_getint(p, "dig");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_hitdie(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_mhp = parser_getint(p, "mhp");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_exp(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->r_exp = parser_getint(p, "exp");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_infravision(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->infra = parser_getint(p, "infra");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_history(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->history = findchart(histories, parser_getuint(p, "hist"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_age(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->b_age = parser_getint(p, "base_age");
	r->m_age = parser_getint(p, "mod_age");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_height(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->base_hgt = parser_getint(p, "base_hgt");
	r->mod_hgt = parser_getint(p, "mod_hgt");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_weight(struct parser *p) {
	struct player_race *r = parser_priv(p);
	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	r->base_wgt = parser_getint(p, "base_wgt");
	r->mod_wgt = parser_getint(p, "mod_wgt");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_obj_flags(struct parser *p) {
	struct player_race *r = parser_priv(p);
	char *flags;
	char *s;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(r->flags, OF_SIZE, list_obj_flag_names, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_play_flags(struct parser *p) {
	struct player_race *r = parser_priv(p);
	char *flags;
	char *s;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(r->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_values(struct parser *p) {
	struct player_race *r = parser_priv(p);
	char *s;
	char *t;

	if (!r)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");

	while (t) {
		int value = 0;
		int index = 0;
		bool found = false;
		if (!grab_index_and_int(&value, &index, list_element_names, "RES_", t)) {
			found = true;
			r->el_info[index].res_level = value;
		}
		if (!found)
			break;

		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

/**
 * Powers, shared between races (PLR-02) and classes (PLR-06).
 *
 * Both declare them the same way and mean the same thing by them, so the
 * parsing lives here once and each parser passes in the list it owns:
 *
 *	power:<name>          opens a new one
 *	power-level:<n>       character level before it can be used
 *	power-cost:<n>        mana, or hit points where there is no mana
 *	power-stat:<STAT>     what makes it more reliable
 *	power-fail:<n>        base failure, in percent
 *	power-when:<from>:<to>  opens a level band; everything after belongs to it
 *	power-effect:...      appended to the current band
 *	power-dice:<dice>     dice for the last effect
 *	power-effect-msg:...  the killer string a DAMAGE effect names
 *	power-expr:...        binds a name in those dice
 */
static struct player_power *power_last(struct player_power *head) {
	while (head && head->next) head = head->next;

	return head;
}

static struct power_effect *power_last_band(struct player_power *power) {
	struct power_effect *band = power ? power->effects : NULL;

	while (band && band->next) band = band->next;

	return band;
}

/** The effect a `power-dice` or `power-expr` line is talking about. */
static struct effect *power_last_effect(struct player_power *power) {
	struct power_effect *band = power_last_band(power);
	struct effect *effect = band ? band->effect : NULL;

	while (effect && effect->next) effect = effect->next;

	return effect;
}

static enum parser_error power_parse_new(struct player_power **head,
										 const char *name) {
	struct player_power *power = mem_zalloc(sizeof(*power));
	struct player_power *last = power_last(*head);

	power->name = string_make(name);
	power->stat = STAT_STR;

	if (last) {
		last->next = power;
	} else {
		*head = power;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error power_parse_when(struct player_power *power,
										  int from, int to) {
	struct power_effect *band, *last;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (from < 0 || to < 0) return PARSE_ERROR_INVALID_VALUE;
	if (to && to < from) return PARSE_ERROR_INVALID_VALUE;

	band = mem_zalloc(sizeof(*band));
	band->from = from;
	band->to = to;

	last = power_last_band(power);
	if (last) {
		last->next = band;
	} else {
		power->effects = band;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error power_parse_effect(struct player_power *power,
											struct parser *p) {
	struct power_effect *band;
	struct effect *effect, *new_effect;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;

	/* Effects before any `power-when` belong to a band that always applies. */
	if (!power->effects) {
		enum parser_error drop = power_parse_when(power, 0, 0);

		if (drop != PARSE_ERROR_NONE) return drop;
	}

	band = power_last_band(power);
	new_effect = mem_zalloc(sizeof(*new_effect));

	if (band->effect) {
		effect = band->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		band->effect = new_effect;
	}

	return grab_effect_data(p, new_effect);
}

static enum parser_error power_parse_dice(struct player_power *power,
										  const char *string) {
	struct effect *effect = power_last_effect(power);
	dice_t *dice;

	if (!effect) return PARSE_ERROR_MISSING_RECORD_HEADER;

	dice = dice_new();
	if (!dice) return PARSE_ERROR_INVALID_DICE;

	if (!dice_parse_string(dice, string)) {
		dice_free(dice);
		return PARSE_ERROR_NOT_RANDOM;
	}

	dice_free(effect->dice);
	effect->dice = dice;

	return PARSE_ERROR_NONE;
}

/**
 * Bind a name in a power's dice to something about the character.
 *
 * The same shape as a class spell's `expr:`, and needed for the same reason: a
 * Draconian's breath and a Mindflayer's blast scale with level in Zangband, and
 * without this they would be flat numbers that stop mattering.
 */
/**
 * The killer string a `DAMAGE` effect in a power chain names.
 *
 * The class-spell parser has had `effect-msg` since 4.2; the power parsers
 * never grew it, so a power that hurt its own user could only report
 * "yourself". Sterilize is the first to need it -- Zangband kills you with
 * "the strain of forcing abstinence" -- and it is the same field on the same
 * struct, so the helper is shared rather than special-cased.
 */
static enum parser_error power_parse_msg(struct player_power *power,
										 const char *text) {
	struct effect *effect = power_last_effect(power);

	if (!effect) return PARSE_ERROR_MISSING_RECORD_HEADER;

	effect->msg = string_append(effect->msg, text);
	return PARSE_ERROR_NONE;
}

static enum parser_error power_parse_expr(struct player_power *power,
										  const char *name, const char *base,
										  const char *expr) {
	struct effect *effect = power_last_effect(power);
	expression_t *expression;
	expression_base_value_f function;
	enum parser_error result;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!effect || !effect->dice) return PARSE_ERROR_NONE;

	expression = expression_new();
	if (!expression) return PARSE_ERROR_INVALID_EXPRESSION;

	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0)
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	else if (dice_bind_expression(effect->dice, name, expression) < 0)
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	else
		result = PARSE_ERROR_NONE;

	expression_free(expression);

	return result;
}

/** Free a power list, whoever owns it. */
static void power_free(struct player_power *power) {
	while (power) {
		struct player_power *pnext = power->next;
		struct power_effect *band = power->effects;

		while (band) {
			struct power_effect *bnext = band->next;

			free_effect(band->effect);
			mem_free(band);
			band = bnext;
		}

		string_free(power->name);
		mem_free(power);
		power = pnext;
	}
}

/* ---- the race's copies of the above (PLR-02) ---- */

static enum parser_error parse_p_race_power(struct parser *p) {
	struct player_race *r = parser_priv(p);

	if (!r) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_new(&r->powers, parser_getstr(p, "name"));
}

static enum parser_error parse_p_race_power_level(struct parser *p) {
	struct player_race *r = parser_priv(p);
	struct player_power *power = r ? power_last(r->powers) : NULL;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	power->level = parser_getint(p, "level");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_power_cost(struct parser *p) {
	struct player_race *r = parser_priv(p);
	struct player_power *power = r ? power_last(r->powers) : NULL;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	power->cost = parser_getint(p, "cost");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_power_fail(struct parser *p) {
	struct player_race *r = parser_priv(p);
	struct player_power *power = r ? power_last(r->powers) : NULL;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	power->fail = parser_getint(p, "fail");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_power_stat(struct parser *p) {
	struct player_race *r = parser_priv(p);
	struct player_power *power = r ? power_last(r->powers) : NULL;
	int stat;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;

	stat = stat_name_to_idx(parser_getsym(p, "stat"));
	if (stat < 0) return PARSE_ERROR_INVALID_VALUE;
	power->stat = stat;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_p_race_power_when(struct parser *p) {
	struct player_race *r = parser_priv(p);

	return power_parse_when(r ? power_last(r->powers) : NULL,
							parser_getint(p, "from"), parser_getint(p, "to"));
}

static enum parser_error parse_p_race_power_effect(struct parser *p) {
	struct player_race *r = parser_priv(p);

	return power_parse_effect(r ? power_last(r->powers) : NULL, p);
}

static enum parser_error parse_p_race_power_dice(struct parser *p) {
	struct player_race *r = parser_priv(p);

	return power_parse_dice(r ? power_last(r->powers) : NULL,
							parser_getstr(p, "dice"));
}

static enum parser_error parse_p_race_power_expr(struct parser *p) {
	struct player_race *r = parser_priv(p);

	return power_parse_expr(r ? power_last(r->powers) : NULL,
							parser_getsym(p, "name"), parser_getsym(p, "base"),
							parser_getstr(p, "expr"));
}

/**
 * The Lords of the Courts of Chaos (ZangbandTK, PLR-05).
 *
 * Two record kinds in one file.  `reward:` blocks come first and define what
 * each favour or punishment does; `patron:` blocks name a Lord and list twenty
 * of those codes, worst first.  The codes are resolved into pointers once the
 * whole file has been read, so a Lord may refer to a reward defined after it.
 */
struct patron *patrons = NULL;
static struct patron_reward *patron_rewards = NULL;

/** Whether the last record read was a reward or a Lord. */
static bool patron_in_reward = false;

static struct patron_reward *patron_reward_by_code(const char *code) {
	struct patron_reward *r;

	for (r = patron_rewards; r; r = r->next)
		if (streq(r->code, code)) return r;

	return NULL;
}

static struct patron_reward *patron_last_reward(void) {
	struct patron_reward *r = patron_rewards;

	while (r && r->next) r = r->next;

	return r;
}

static struct patron *patron_last(void) {
	struct patron *pa = patrons;

	while (pa && pa->next) pa = pa->next;

	return pa;
}

static enum parser_error parse_patron_reward(struct parser *p) {
	struct patron_reward *reward = mem_zalloc(sizeof(*reward));
	struct patron_reward *last = patron_last_reward();

	reward->code = string_make(parser_getsym(p, "code"));

	/*
	 * The message is handed to msg() with the Lord's name, so like a martial
	 * blow's it has to be exactly one %s and nothing else.  Checked rather
	 * than trusted: a second conversion here reads an argument that was never
	 * passed.
	 */
	{
		const char *m = parser_getstr(p, "message");
		const char *c;
		int subs = 0;

		for (c = m; *c; c++) {
			if (*c != '%') continue;
			if (c[1] == '%') { c++; continue; }
			if (c[1] != 's') { string_free(reward->code); mem_free(reward);
				return PARSE_ERROR_INVALID_VALUE; }
			subs++;
			c++;
		}

		if (subs != 1) {
			string_free(reward->code);
			mem_free(reward);
			return PARSE_ERROR_INVALID_VALUE;
		}

		reward->message = string_make(m);
	}

	if (patron_reward_by_code(reward->code)) {
		string_free(reward->code);
		string_free(reward->message);
		mem_free(reward);
		return PARSE_ERROR_INVALID_VALUE;
	}

	if (last) {
		last->next = reward;
	} else {
		patron_rewards = reward;
	}

	patron_in_reward = true;
	parser_setpriv(p, reward);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_patron_reward_effect(struct parser *p) {
	struct patron_reward *reward = patron_last_reward();
	struct effect *effect, *new_effect;

	if (!reward || !patron_in_reward) return PARSE_ERROR_MISSING_RECORD_HEADER;

	new_effect = mem_zalloc(sizeof(*new_effect));
	if (reward->effect) {
		effect = reward->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		reward->effect = new_effect;
	}

	return grab_effect_data(p, new_effect);
}

static struct effect *patron_last_effect(void) {
	struct patron_reward *reward = patron_last_reward();
	struct effect *effect = reward ? reward->effect : NULL;

	while (effect && effect->next) effect = effect->next;

	return effect;
}

static enum parser_error parse_patron_reward_dice(struct parser *p) {
	struct effect *effect = patron_last_effect();
	dice_t *dice;

	if (!effect || !patron_in_reward) return PARSE_ERROR_MISSING_RECORD_HEADER;

	dice = dice_new();
	if (!dice) return PARSE_ERROR_INVALID_DICE;

	if (!dice_parse_string(dice, parser_getstr(p, "dice"))) {
		dice_free(dice);
		return PARSE_ERROR_NOT_RANDOM;
	}

	dice_free(effect->dice);
	effect->dice = dice;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_patron_reward_expr(struct parser *p) {
	struct effect *effect = patron_last_effect();
	expression_t *expression;
	expression_base_value_f function;
	enum parser_error result;

	if (!effect || !patron_in_reward) return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!effect->dice) return PARSE_ERROR_NONE;

	expression = expression_new();
	if (!expression) return PARSE_ERROR_INVALID_EXPRESSION;

	function = effect_value_base_by_name(parser_getsym(p, "base"));
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression,
										 parser_getstr(p, "expr")) < 0)
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	else if (dice_bind_expression(effect->dice, parser_getsym(p, "name"),
								  expression) < 0)
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	else
		result = PARSE_ERROR_NONE;

	expression_free(expression);

	return result;
}

static enum parser_error parse_patron_name(struct parser *p) {
	struct patron *patron = mem_zalloc(sizeof(*patron));
	struct patron *last = patron_last();

	patron->name = string_make(parser_getstr(p, "name"));

	if (last) {
		last->next = patron;
	} else {
		patrons = patron;
	}

	patron_in_reward = false;
	parser_setpriv(p, patron);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_patron_title(struct parser *p) {
	struct patron *patron = patron_last();

	if (!patron || patron_in_reward) return PARSE_ERROR_MISSING_RECORD_HEADER;
	string_free(patron->title);
	patron->title = string_make(parser_getstr(p, "title"));

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_patron_desc(struct parser *p) {
	struct patron *patron = patron_last();

	if (!patron || patron_in_reward) return PARSE_ERROR_MISSING_RECORD_HEADER;
	patron->text = string_append(patron->text, parser_getstr(p, "desc"));

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_patron_ladder(struct parser *p) {
	struct patron *patron = patron_last();
	char *list, *code;
	int n = 0;

	if (!patron || patron_in_reward) return PARSE_ERROR_MISSING_RECORD_HEADER;

	list = string_make(parser_getstr(p, "rewards"));

	for (code = strtok(list, "| "); code; code = strtok(NULL, "| ")) {
		if (n >= PATRON_LADDER) {
			string_free(list);
			return PARSE_ERROR_TOO_MANY_ENTRIES;
		}
		patron->ladder_codes[n++] = string_make(code);
	}

	string_free(list);

	/*
	 * Exactly twenty, because the roll that indexes this assumes the whole
	 * ladder is there.  A short list would silently give some Lord a run of
	 * nothing at the generous end, which is the sort of thing nobody would
	 * notice for months.
	 */
	if (n != PATRON_LADDER) return PARSE_ERROR_INVALID_VALUE;

	patron->nladder = n;

	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_patron(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	patron_in_reward = false;

	parser_reg(p, "reward sym code str message", parse_patron_reward);
	parser_reg(p, "reward-effect sym eff ?sym type ?int radius ?int other",
			   parse_patron_reward_effect);
	parser_reg(p, "reward-dice str dice", parse_patron_reward_dice);
	parser_reg(p, "reward-expr sym name sym base str expr",
			   parse_patron_reward_expr);
	parser_reg(p, "patron str name", parse_patron_name);
	parser_reg(p, "patron-title str title", parse_patron_title);
	parser_reg(p, "patron-desc str desc", parse_patron_desc);
	parser_reg(p, "patron-rewards str rewards", parse_patron_ladder);

	return p;
}

static errr run_parse_patron(struct parser *p) {
	return parse_file_quit_not_found(p, "patron");
}

static errr finish_parse_patron(struct parser *p) {
	struct patron *patron;
	int i;

	/* Resolve every ladder entry, now that all the rewards have been read. */
	for (patron = patrons; patron; patron = patron->next) {
		/*
		 * A patron whose `patron-rewards` line is missing or misspelled never
		 * reached parse_patron_ladder, so its codes are all NULL -- which
		 * without this check reaches strcmp(x, NULL) and takes the game down
		 * during data loading, before there is any UI to say why.  The short
		 * list is already rejected; the absent one was not.
		 */
		if (patron->nladder != PATRON_LADDER) {
			parser_destroy(p);
			return PARSE_ERROR_MISSING_FIELD;
		}

		for (i = 0; i < PATRON_LADDER; i++) {
			patron->ladder[i] = patron_reward_by_code(patron->ladder_codes[i]);

			if (!patron->ladder[i]) {
				parser_destroy(p);
				return PARSE_ERROR_INVALID_VALUE;
			}
		}
	}

	parser_destroy(p);

	return 0;
}

static void cleanup_patron(void) {
	struct patron *patron = patrons;
	struct patron_reward *reward = patron_rewards;

	while (patron) {
		struct patron *next = patron->next;
		int i;

		for (i = 0; i < PATRON_LADDER; i++)
			string_free(patron->ladder_codes[i]);

		string_free(patron->name);
		string_free(patron->title);
		string_free(patron->text);
		mem_free(patron);
		patron = next;
	}
	patrons = NULL;

	while (reward) {
		struct patron_reward *next = reward->next;

		string_free(reward->code);
		string_free(reward->message);
		free_effect(reward->effect);
		mem_free(reward);
		reward = next;
	}
	patron_rewards = NULL;
}

struct file_parser patron_parser = {
	"patron",
	init_parse_patron,
	run_parse_patron,
	finish_parse_patron,
	cleanup_patron
};

static struct parser *init_parse_p_race(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_p_race_name);
	parser_reg(p, "virtues str virtues", parse_p_race_virtues);
	parser_reg(p, "mutation-affinity sym code int chance", parse_p_race_mutation);
	parser_reg(p, "mutation-rate int birth int level", parse_p_race_mutation_rate);
	parser_reg(p, "stats int str int int int wis int dex int con", parse_p_race_stats);
	parser_reg(p, "skill-disarm-phys int disarm", parse_p_race_skill_disarm_phys);
	parser_reg(p, "skill-disarm-magic int disarm", parse_p_race_skill_disarm_magic);
	parser_reg(p, "skill-device int device", parse_p_race_skill_device);
	parser_reg(p, "skill-save int save", parse_p_race_skill_save);
	parser_reg(p, "skill-stealth int stealth", parse_p_race_skill_stealth);
	parser_reg(p, "skill-search int search", parse_p_race_skill_search);
	parser_reg(p, "skill-melee int melee", parse_p_race_skill_melee);
	parser_reg(p, "skill-shoot int shoot", parse_p_race_skill_shoot);
	parser_reg(p, "skill-throw int throw", parse_p_race_skill_throw);
	parser_reg(p, "skill-dig int dig", parse_p_race_skill_dig);
	parser_reg(p, "hitdie int mhp", parse_p_race_hitdie);
	parser_reg(p, "exp int exp", parse_p_race_exp);
	parser_reg(p, "infravision int infra", parse_p_race_infravision);
	parser_reg(p, "history uint hist", parse_p_race_history);
	parser_reg(p, "age int base_age int mod_age", parse_p_race_age);
	parser_reg(p, "height int base_hgt int mod_hgt", parse_p_race_height);
	parser_reg(p, "weight int base_wgt int mod_wgt", parse_p_race_weight);
	parser_reg(p, "obj-flags ?str flags", parse_p_race_obj_flags);
	parser_reg(p, "player-flags ?str flags", parse_p_race_play_flags);
	parser_reg(p, "values str values", parse_p_race_values);
	parser_reg(p, "power str name", parse_p_race_power);
	parser_reg(p, "power-level int level", parse_p_race_power_level);
	parser_reg(p, "power-cost int cost", parse_p_race_power_cost);
	parser_reg(p, "power-stat sym stat", parse_p_race_power_stat);
	parser_reg(p, "power-fail int fail", parse_p_race_power_fail);
	parser_reg(p, "power-effect sym eff ?sym type ?int radius ?int other",
			   parse_p_race_power_effect);
	parser_reg(p, "power-dice str dice", parse_p_race_power_dice);
	parser_reg(p, "power-when int from int to", parse_p_race_power_when);
	parser_reg(p, "power-expr sym name sym base str expr",
			   parse_p_race_power_expr);
	return p;
}

static errr run_parse_p_race(struct parser *p) {
	return parse_file_quit_not_found(p, "p_race");
}

static errr finish_parse_p_race(struct parser *p) {
	struct player_race *r;
	int num = 0;
	races = parser_priv(p);
	for (r = races; r; r = r->next) num++;
	for (r = races; r; r = r->next, num--) {
		assert(num);
		r->ridx = num - 1;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_p_race(void)
{
	struct player_race *p = races;
	struct player_race *next;

	while (p) {
		next = p->next;

		/* And whatever the race could do (PLR-02). */
		power_free(p->powers);

		string_free((char *)p->name);
		mem_free(p);
		p = next;
	}
}

struct file_parser p_race_parser = {
	"p_race",
	init_parse_p_race,
	run_parse_p_race,
	finish_parse_p_race,
	cleanup_p_race
};

/**
 * ------------------------------------------------------------------------
 * Initialize renames
 * ------------------------------------------------------------------------ */

/**
 * What this game used to call things (rename.txt).
 *
 * Consulted only when a savefile names something no lookup can find, so it
 * costs nothing for a current file and turns a rename from a lost object into
 * no loss at all.  Kept as a flat list because it is walked once per failure
 * and there are never many.
 */
struct rename_entry *renames = NULL;

static enum parser_error parse_rename_object(struct parser *p) {
	struct rename_entry *r = mem_zalloc(sizeof *r);
	const char *to = parser_getstr(p, "to");

	r->kind = RENAME_OBJECT;
	r->tval = tval_find_idx(parser_getsym(p, "tval"));
	if (r->tval < 0) {
		mem_free(r);
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	r->from = string_make(parser_getsym(p, "from"));
	r->to = streq(to, "-") ? NULL : string_make(to);
	r->next = renames;
	renames = r;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_rename_named(struct parser *p,
		enum rename_kind kind) {
	struct rename_entry *r = mem_zalloc(sizeof *r);
	const char *to = parser_getstr(p, "to");

	r->kind = kind;
	r->tval = -1;
	r->from = string_make(parser_getsym(p, "from"));
	r->to = streq(to, "-") ? NULL : string_make(to);
	r->next = renames;
	renames = r;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_rename_monster(struct parser *p) {
	return parse_rename_named(p, RENAME_MONSTER);
}

static enum parser_error parse_rename_artifact(struct parser *p) {
	return parse_rename_named(p, RENAME_ARTIFACT);
}

static enum parser_error parse_rename_ego(struct parser *p) {
	return parse_rename_named(p, RENAME_EGO);
}

static struct parser *init_parse_rename(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "object sym tval sym from str to", parse_rename_object);
	parser_reg(p, "monster sym from str to", parse_rename_monster);
	parser_reg(p, "artifact sym from str to", parse_rename_artifact);
	parser_reg(p, "ego sym from str to", parse_rename_ego);
	return p;
}

static errr run_parse_rename(struct parser *p) {
	return parse_file_quit_not_found(p, "rename");
}

static errr finish_parse_rename(struct parser *p) {
	parser_destroy(p);
	return 0;
}

static void cleanup_rename(void) {
	struct rename_entry *r = renames;

	while (r) {
		struct rename_entry *next = r->next;

		string_free(r->from);
		if (r->to) string_free(r->to);
		mem_free(r);
		r = next;
	}
	renames = NULL;
}

struct file_parser rename_parser = {
	"rename",
	init_parse_rename,
	run_parse_rename,
	finish_parse_rename,
	cleanup_rename
};

/**
 * ------------------------------------------------------------------------
 * Initialize player magic realms
 * ------------------------------------------------------------------------ */
static enum parser_error parse_realm_name(struct parser *p) {
	struct magic_realm *h = parser_priv(p);
	struct magic_realm *realm = mem_zalloc(sizeof *realm);
	const char *name = parser_getstr(p, "name");

	realm->next = h;
	parser_setpriv(p, realm);
	realm->name = string_make(name);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_stat(struct parser *p) {
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	realm->stat = stat_name_to_idx(parser_getsym(p, "stat"));
	if (realm->stat < 0) {
		return PARSE_ERROR_INVALID_SPELL_STAT;
	}
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_verb(struct parser *p) {
	const char *verb = parser_getstr(p, "verb");
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(realm->verb);
	realm->verb = string_make(verb);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_spell_noun(struct parser *p) {
	const char *spell = parser_getstr(p, "spell");
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(realm->spell_noun);
	realm->spell_noun = string_make(spell);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_realm_book_noun(struct parser *p) {
	const char *book = parser_getstr(p, "book");
	struct magic_realm *realm = parser_priv(p);

	if (!realm) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	string_free(realm->book_noun);
	realm->book_noun = string_make(book);
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_realm(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_realm_name);
	parser_reg(p, "virtues str virtues", parse_realm_virtues);
	parser_reg(p, "stat sym stat", parse_realm_stat);
	parser_reg(p, "verb str verb", parse_realm_verb);
	parser_reg(p, "spell-noun str spell", parse_realm_spell_noun);
	parser_reg(p, "book-noun str book", parse_realm_book_noun);
	return p;
}

static errr run_parse_realm(struct parser *p) {
	return parse_file_quit_not_found(p, "realm");
}

static errr finish_parse_realm(struct parser *p) {
	struct magic_realm *r;
	unsigned int count = 0;

	realms = parser_priv(p);
	parser_destroy(p);

	/*
	 * Reversed into file order first, and then numbered.
	 *
	 * The parser links each record to the one before it, so the list arrives
	 * back-to-front: without this, index 0 is Trump because Trump is last in
	 * the file, and every list of realms the player is shown would run
	 * backwards.
	 *
	 * `ridx` is an in-memory subscript into `realm_allowed[]` and nothing
	 * else -- a character's realms go into the savefile by *name*, as the
	 * patron and the virtues do, so that inserting a realm cannot rebind a
	 * saved character to a different one. Keeping the index in file order is
	 * for the reader's sake, not the savefile's.
	 */
	{
		struct magic_realm *head = NULL, *next;

		for (r = realms; r; r = next) {
			next = r->next;
			r->next = head;
			head = r;
		}
		realms = head;
	}

	for (r = realms; r; r = r->next) count++;

	/*
	 * The safety requirement is that `realm_allowed[]` can hold an index for
	 * every realm, and that is all this checks. Asserting the count is exactly
	 * seven belongs to `player/realm`, which reads the shipped data; the parse
	 * suites supply their own cut-down realm.txt and a hard equality here would
	 * fail them for having two realms, which is not a defect.
	 */
	if (count > REALM_MAX) {
		quit_fmt("realm.txt has %u realms; REALM_MAX is %d.", count,
				 REALM_MAX);
	}

	count = 0;
	for (r = realms; r; r = r->next) r->ridx = count++;

	return 0;
}

static void cleanup_realm(void)
{
	struct magic_realm *p = realms;
	struct magic_realm *next;

	while (p) {
		next = p->next;
		string_free(p->name);
		string_free(p->verb);
		string_free(p->spell_noun);
		string_free(p->book_noun);
		mem_free(p);
		p = next;
	}
}

/**
 * Chaos mutations (ZangbandTK, PLR-13).
 *
 * The one Zangband system that never had a data file: its 96 mutations are a
 * C array in tables.c and their selection weighting is the shape of a switch
 * statement. `zconv mutations` reads both and writes mutation.txt, which this
 * parses.
 */
struct mutation *mutations = NULL;

static enum parser_error parse_mutation_name(struct parser *p) {
	struct mutation *h = parser_priv(p);
	struct mutation *m = mem_zalloc(sizeof(*m));

	m->next = h;
	m->name = string_make(parser_getstr(p, "name"));
	m->stat = -1;
	m->blow_element = -1;
	parser_setpriv(p, m);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_kind(struct parser *p) {
	struct mutation *m = parser_priv(p);
	const char *kind = parser_getstr(p, "kind");

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	if (streq(kind, "activatable")) m->kind = MUTATION_KIND_ACTIVATABLE;
	else if (streq(kind, "random")) m->kind = MUTATION_KIND_RANDOM;
	else if (streq(kind, "continuous")) m->kind = MUTATION_KIND_CONTINUOUS;
	else if (streq(kind, "melee")) m->kind = MUTATION_KIND_MELEE;
	else return PARSE_ERROR_INVALID_VALUE;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_level(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->level = parser_getint(p, "level");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_cost(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->cost = parser_getint(p, "cost");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_stat(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->stat = stat_name_to_idx(parser_getsym(p, "stat"));
	if (m->stat < 0) return PARSE_ERROR_INVALID_SPELL_STAT;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_difficulty(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->difficulty = parser_getint(p, "difficulty");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_chance(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->chance = parser_getint(p, "chance");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_weight(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->weight = parser_getint(p, "weight");
	return PARSE_ERROR_NONE;
}

/*
 * A prerequisite, of which Zangband has exactly three and documents none:
 * the Midas touch wants a thousand gold per level in hand, and a silly voice
 * and elemental vulnerability want three mutations already
 * ([mutation.c:150](../archive/zangband/src/mutation.c#L150)).
 */
static enum parser_error parse_mutation_requires(struct parser *p) {
	struct mutation *m = parser_priv(p);
	const char *req = parser_getstr(p, "requires");

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	if (streq(req, "gold")) {
		m->gate = MUTATION_GATE_GOLD;
	} else if (!strncmp(req, "mutations:", 10)) {
		m->gate = MUTATION_GATE_MUTATIONS;
		m->gate_value = atoi(req + 10);
	} else {
		return PARSE_ERROR_INVALID_VALUE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_power(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->power = string_make(parser_getstr(p, "power"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_desc(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->desc = string_append(m->desc, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_gain(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->gain = string_append(m->gain, parser_getstr(p, "gain"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_lose(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->lose = string_append(m->lose, parser_getstr(p, "lose"));
	return PARSE_ERROR_NONE;
}

/**
 * The mutation's power, made on first mention.
 *
 * A mutation's level, cost, stat and failure are already on the record by the
 * time its effects arrive -- they come out of Zangband's table rather than out
 * of mutmap.toml -- so the power is filled in from those rather than from
 * `power-level` lines of its own. `difficulty` becomes `fail` directly, which
 * is the reading PLR-02 took for the racial powers: Zangband's own success
 * roll is a different shape entirely, and the Vampire's `drink blood` already
 * ships with this mutation's 9 as its failure percentage.
 *
 * Made here rather than in the effect parser because a power whose radius
 * changes with level opens with `power-when` and not with `power-effect`.
 */
static struct player_power *mutation_power(struct mutation *m) {
	if (!m) return NULL;

	if (!m->action) {
		m->action = mem_zalloc(sizeof(*m->action));
		m->action->name = string_make(m->power ? m->power : m->name);
		m->action->level = m->level;
		m->action->cost = m->cost;
		m->action->stat = m->stat;
		m->action->fail = m->difficulty;
	}

	return m->action;
}

static enum parser_error parse_mutation_power_when(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_when(mutation_power(m), parser_getint(p, "from"),
							parser_getint(p, "to"));
}

static enum parser_error parse_mutation_power_effect(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_effect(mutation_power(m), p);
}

static enum parser_error parse_mutation_power_dice(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m || !m->action) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_dice(m->action, parser_getstr(p, "dice"));
}

static enum parser_error parse_mutation_power_msg(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m || !m->action) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_msg(m->action, parser_getstr(p, "text"));
}

static enum parser_error parse_mutation_power_expr(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m || !m->action) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_expr(m->action, parser_getsym(p, "name"),
							parser_getsym(p, "base"),
							parser_getstr(p, "expr"));
}

static struct player_power *mutation_fires(struct mutation *m) {
	if (!m) return NULL;

	if (!m->fires) {
		m->fires = mem_zalloc(sizeof(*m->fires));
		m->fires->name = string_make(m->name);
	}

	return m->fires;
}

static enum parser_error parse_mutation_fires_when(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_when(mutation_fires(m), parser_getint(p, "from"),
							parser_getint(p, "to"));
}

static enum parser_error parse_mutation_fires_effect(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_effect(mutation_fires(m), p);
}

static enum parser_error parse_mutation_fires_dice(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m || !m->fires) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_dice(m->fires, parser_getstr(p, "dice"));
}

static enum parser_error parse_mutation_fires_expr(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m || !m->fires) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_expr(m->fires, parser_getsym(p, "name"),
							parser_getsym(p, "base"),
							parser_getstr(p, "expr"));
}

static enum parser_error parse_mutation_blow_dice(struct parser *p) {
	struct mutation *m = parser_priv(p);
	dice_t *dice;
	const char *string;

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	dice = dice_new();
	if (!dice) return PARSE_ERROR_INVALID_DICE;

	string = parser_getstr(p, "dice");
	if (!dice_parse_string(dice, string)) {
		dice_free(dice);
		return PARSE_ERROR_NOT_RANDOM;
	}
	dice_random_value(dice, &m->blow);
	dice_free(dice);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_blow_weight(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->blow_weight = parser_getint(p, "weight");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_blow_verb(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	string_free(m->blow_verb);
	m->blow_verb = string_make(parser_getstr(p, "verb"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_blow_element(struct parser *p) {
	struct mutation *m = parser_priv(p);
	int i;

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	i = proj_name_to_idx(parser_getstr(p, "element"));
	if (i < 0) return PARSE_ERROR_INVALID_VALUE;

	m->blow_element = i;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_unavailable(struct parser *p) {
	struct mutation *m = parser_priv(p);
	const char *why = parser_getstr(p, "why");

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	if (streq(why, "rejected")) {
		m->refused = true;
	} else if (!streq(why, "deferred")) {
		return PARSE_ERROR_INVALID_VALUE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_armour(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->armour = parser_getint(p, "armour");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_save(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	m->save = parser_getint(p, "save");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_save_scale(struct parser *p) {
	struct mutation *m = parser_priv(p);

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (parser_getint(p, "scale") <= 0) return PARSE_ERROR_INVALID_VALUE;
	m->save_scale = parser_getint(p, "scale");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_values(struct parser *p) {
	struct mutation *m = parser_priv(p);
	char *s, *t;

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");
	while (t) {
		int value = 0, index = 0;
		bool found = false;

		if (!grab_index_and_int(&value, &index, obj_mods, "", t)) {
			found = true;
			m->modifiers[index] = value;
		}
		if (!grab_index_and_int(&value, &index, list_element_names, "RES_",
								t)) {
			found = true;
			m->el_info[index] = value;
		}
		if (!found) break;

		t = strtok(NULL, " |");
	}
	string_free(s);

	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_suppresses(struct parser *p) {
	struct mutation *m = parser_priv(p);
	char *s, *t;

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	s = string_make(parser_getstr(p, "flags"));
	t = strtok(s, " |");
	while (t) {
		if (grab_flag(m->suppress, OF_SIZE, list_obj_flag_names, t)) break;
		t = strtok(NULL, " |");
	}
	string_free(s);

	return t ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_mutation_flags(struct parser *p) {
	struct mutation *m = parser_priv(p);
	char *s, *t;

	if (!m) return PARSE_ERROR_MISSING_RECORD_HEADER;

	s = string_make(parser_getstr(p, "flags"));
	t = strtok(s, " |");
	while (t) {
		if (grab_flag(m->flags, OF_SIZE, list_obj_flag_names, t)) break;
		t = strtok(NULL, " |");
	}
	string_free(s);

	return t ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static struct parser *init_parse_mutation(void) {
	struct parser *p = parser_new();

	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_mutation_name);
	parser_reg(p, "kind str kind", parse_mutation_kind);
	parser_reg(p, "level int level", parse_mutation_level);
	parser_reg(p, "cost int cost", parse_mutation_cost);
	parser_reg(p, "stat sym stat", parse_mutation_stat);
	parser_reg(p, "difficulty int difficulty", parse_mutation_difficulty);
	parser_reg(p, "chance int chance", parse_mutation_chance);
	parser_reg(p, "weight int weight", parse_mutation_weight);
	parser_reg(p, "requires str requires", parse_mutation_requires);
	parser_reg(p, "armour int armour", parse_mutation_armour);
	parser_reg(p, "save int save", parse_mutation_save);
	parser_reg(p, "save-scale int scale", parse_mutation_save_scale);
	parser_reg(p, "power-when int from int to", parse_mutation_power_when);
	parser_reg(p, "power-effect sym eff ?sym type ?int radius ?int other",
			   parse_mutation_power_effect);
	parser_reg(p, "power-dice str dice", parse_mutation_power_dice);
	parser_reg(p, "power-effect-msg str text", parse_mutation_power_msg);
	parser_reg(p, "power-expr sym name sym base str expr",
			   parse_mutation_power_expr);
	parser_reg(p, "fires-when int from int to", parse_mutation_fires_when);
	parser_reg(p, "fires-effect sym eff ?sym type ?int radius ?int other",
			   parse_mutation_fires_effect);
	parser_reg(p, "fires-dice str dice", parse_mutation_fires_dice);
	parser_reg(p, "fires-expr sym name sym base str expr",
			   parse_mutation_fires_expr);
	parser_reg(p, "blow-dice str dice", parse_mutation_blow_dice);
	parser_reg(p, "blow-weight int weight", parse_mutation_blow_weight);
	parser_reg(p, "blow-verb str verb", parse_mutation_blow_verb);
	parser_reg(p, "blow-element str element", parse_mutation_blow_element);
	parser_reg(p, "values str values", parse_mutation_values);
	parser_reg(p, "flags str flags", parse_mutation_flags);
	parser_reg(p, "suppresses str flags", parse_mutation_suppresses);
	parser_reg(p, "unavailable str why", parse_mutation_unavailable);
	parser_reg(p, "power str power", parse_mutation_power);
	parser_reg(p, "desc str desc", parse_mutation_desc);
	parser_reg(p, "gain str gain", parse_mutation_gain);
	parser_reg(p, "lose str lose", parse_mutation_lose);

	return p;
}

static errr run_parse_mutation(struct parser *p) {
	return parse_file_quit_not_found(p, "mutation");
}

static errr finish_parse_mutation(struct parser *p) {
	struct mutation *m, *n;
	unsigned int count = 0;

	/* The list is built backwards; number it in file order. */
	for (m = parser_priv(p); m; m = m->next) count++;

	if (count > MUTATION_MAX) {
		quit_fmt("mutation.txt has %u entries; MUTATION_MAX is %d.",
				 count, MUTATION_MAX);
	}

	mutations = parser_priv(p);
	for (m = mutations, n = NULL; m; m = m->next) {
		m->midx = --count;
		n = m;
	}
	(void) n;

	parser_destroy(p);
	return 0;
}

static void cleanup_mutation(void) {
	struct mutation *m = mutations;

	while (m) {
		struct mutation *next = m->next;

		string_free(m->name);
		string_free(m->desc);
		string_free(m->gain);
		string_free(m->lose);
		string_free(m->power);
		string_free(m->blow_verb);
		power_free(m->action);
		power_free(m->fires);
		mem_free(m);
		m = next;
	}
	mutations = NULL;
}

struct file_parser mutation_parser = {
	"mutation",
	init_parse_mutation,
	run_parse_mutation,
	finish_parse_mutation,
	cleanup_mutation
};

struct file_parser realm_parser = {
	"realm",
	init_parse_realm,
	run_parse_realm,
	finish_parse_realm,
	cleanup_realm
};

/**
 * ------------------------------------------------------------------------
 * Initialize player shapechange shapes
 * ------------------------------------------------------------------------ */

static enum parser_error parse_shape_name(struct parser *p) {
	struct player_shape *h = parser_priv(p);
	struct player_shape *shape = mem_zalloc(sizeof *shape);

	shape->next = h;
	shape->name = string_make(parser_getstr(p, "name"));
	parser_setpriv(p, shape);
	shape->sidx = z_info->shape_max++;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_combat(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	shape->to_h = parser_getint(p, "to-h");
	shape->to_d = parser_getint(p, "to-d");
	shape->to_a = parser_getint(p, "to-a");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_disarm_phys(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_DISARM_PHYS] = parser_getint(p, "disarm");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_disarm_magic(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_DISARM_MAGIC] = parser_getint(p, "disarm");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_save(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_SAVE] = parser_getint(p, "save");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_stealth(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_STEALTH] = parser_getint(p, "stealth");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_search(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_SEARCH] = parser_getint(p, "search");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_melee(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "melee");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_throw(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_TO_HIT_THROW] = parser_getint(p, "throw");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_skill_dig(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	shape->skills[SKILL_DIGGING] = parser_getint(p, "dig");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_obj_flags(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	char *flags;
	char *s;

	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(shape->flags, OF_SIZE, list_obj_flag_names, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_play_flags(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	char *flags;
	char *s;

	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(shape->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}
	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_values(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	char *s;
	char *t;

	if (!shape)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	s = string_make(parser_getstr(p, "values"));
	t = strtok(s, " |");

	while (t) {
		int value = 0;
		int index = 0;
		bool found = false;
		if (!grab_int_value(shape->modifiers, obj_mods, t))
			found = true;
		if (!grab_index_and_int(&value, &index, list_element_names, "RES_", t)) {
			found = true;
			shape->el_info[index].res_level = value;
		}
		if (!found)
			break;

		t = strtok(NULL, " |");
	}

	string_free(s);
	return t ? PARSE_ERROR_INVALID_VALUE : PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_effect(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect, *new_effect;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*effect));
	if (shape->effect) {
		effect = shape->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		shape->effect = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_shape_effect_yx(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_dice(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;
	dice_t *dice;
	const char *string;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_expr(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}
	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_shape_effect_msg(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct effect *effect;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	/* If there is no effect, assume that this is human and not parser error. */
	effect = shape->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	effect->msg = string_append(effect->msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_shape_blow(struct parser *p) {
	struct player_shape *shape = parser_priv(p);
	struct player_blow *blow;

	if (!shape) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	blow = mem_zalloc(sizeof(*blow));
	blow->name = string_make(parser_getstr(p, "blow"));
	blow->next = shape->blows;
	shape->blows = blow;
	shape->num_blows++;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_shape(void) {
	struct parser *p = parser_new();
	z_info->shape_max = 0;
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_shape_name);
	parser_reg(p, "combat int to-h int to-d int to-a", parse_shape_combat);
	parser_reg(p, "skill-disarm-phys int disarm", parse_shape_skill_disarm_phys);
	parser_reg(p, "skill-disarm-magic int disarm", parse_shape_skill_disarm_magic);
	parser_reg(p, "skill-save int save", parse_shape_skill_save);
	parser_reg(p, "skill-stealth int stealth", parse_shape_skill_stealth);
	parser_reg(p, "skill-search int search", parse_shape_skill_search);
	parser_reg(p, "skill-melee int melee", parse_shape_skill_melee);
	parser_reg(p, "skill-throw int throw", parse_shape_skill_throw);
	parser_reg(p, "skill-dig int dig", parse_shape_skill_dig);
	parser_reg(p, "obj-flags ?str flags", parse_shape_obj_flags);
	parser_reg(p, "player-flags ?str flags", parse_shape_play_flags);
	parser_reg(p, "values str values", parse_shape_values);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_shape_effect);
	parser_reg(p, "effect-yx int y int x", parse_shape_effect_yx);
	parser_reg(p, "dice str dice", parse_shape_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_shape_expr);
	parser_reg(p, "effect-msg str text", parse_shape_effect_msg);
	parser_reg(p, "blow str blow", parse_shape_blow);
	return p;
}

static errr run_parse_shape(struct parser *p) {
	return parse_file_quit_not_found(p, "shape");
}

static errr finish_parse_shape(struct parser *p) {
	shapes = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_shape(void)
{
	struct player_shape *shape = shapes;
	struct player_shape *next;

	while (shape) {
		struct player_blow *blow = shape->blows;
		next = shape->next;
		string_free((char *)shape->name);
		free_effect(shape->effect);
		while (blow) {
			struct player_blow *next_blow = blow->next;
			string_free(blow->name);
			mem_free(blow);
			blow = next_blow;
		}
		mem_free(shape);
		shape = next;
	}
}

struct file_parser shape_parser = {
	"shape",
	init_parse_shape,
	run_parse_shape,
	finish_parse_shape,
	cleanup_shape
};

/**
 * ------------------------------------------------------------------------
 * Initialize player classes
 * ------------------------------------------------------------------------ */

/*
 * Used to remember the maximum number of books for the current class and the
 * maximum number of spells in the current book while parsing so bounds
 * checking can be done.
 */
static int class_max_books = 0;
static int book_max_spells = 0;

static enum parser_error parse_class_name(struct parser *p) {
	struct player_class *h = parser_priv(p);
	struct player_class *c = mem_zalloc(sizeof *c);
	c->name = string_make(parser_getstr(p, "name"));
	c->next = h;
	parser_setpriv(p, c);
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_stats(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	c->c_adj[STAT_STR] = parser_getint(p, "str");
	c->c_adj[STAT_INT] = parser_getint(p, "int");
	c->c_adj[STAT_WIS] = parser_getint(p, "wis");
	c->c_adj[STAT_DEX] = parser_getint(p, "dex");
	c->c_adj[STAT_CON] = parser_getint(p, "con");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_disarm_phys(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DISARM_PHYS] = parser_getint(p, "base");
	c->x_skills[SKILL_DISARM_PHYS] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_disarm_magic(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "base");
	c->x_skills[SKILL_DISARM_MAGIC] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_device(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DEVICE] = parser_getint(p, "base");
	c->x_skills[SKILL_DEVICE] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_save(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_SAVE] = parser_getint(p, "base");
	c->x_skills[SKILL_SAVE] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_stealth(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_STEALTH] = parser_getint(p, "base");
	c->x_skills[SKILL_STEALTH] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_search(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_SEARCH] = parser_getint(p, "base");
	c->x_skills[SKILL_SEARCH] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_melee(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "base");
	c->x_skills[SKILL_TO_HIT_MELEE] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_shoot(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "base");
	c->x_skills[SKILL_TO_HIT_BOW] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_throw(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "base");
	c->x_skills[SKILL_TO_HIT_THROW] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_skill_dig(struct parser *p) {
	struct player_class *c = parser_priv(p);
	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_skills[SKILL_DIGGING] = parser_getint(p, "base");
	c->x_skills[SKILL_DIGGING] = parser_getint(p, "incr");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_hitdie(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_mhp = parser_getint(p, "mhp");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_exp(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->c_exp = parser_getint(p, "exp");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_max_attacks(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->max_attacks = parser_getint(p, "max-attacks");
	return PARSE_ERROR_NONE;
}

/**
 * How many pets the class keeps for free (ZangbandTK, PLR-30).
 */
static enum parser_error parse_class_pet_upkeep(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->pet_upkeep_div = parser_getint(p, "pet-upkeep-div");
	if (c->pet_upkeep_div < 1)
		return PARSE_ERROR_INVALID_VALUE;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_min_weight(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->min_weight = parser_getint(p, "min-weight");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_str_mult(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	c->att_multiply = parser_getint(p, "att-multiply");
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_title(struct parser *p) {
	struct player_class *c = parser_priv(p);
	int n, i;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	n = (int) N_ELEMENTS(c->title);
	for (i = 0; i < n; i++) {
		if (!c->title[i]) {
			c->title[i] = string_make(parser_getstr(p, "title"));
			break;
		}
	}

	return (i >= n) ? PARSE_ERROR_TOO_MANY_ENTRIES : PARSE_ERROR_NONE;
}

static int lookup_option(const char *name)
{
	int result = 1;

	while (1) {
		if (result >= OPT_MAX) {
			return 0;
		}
		if (streq(option_name(result), name)) {
			return result;
		}
		++result;
	}
}

static enum parser_error parse_class_equip(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct start_item *si;
	int tval, sval;
	char *eopts;
	char *s;
	int *einds;
	int nind, nalloc;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;

	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0)
		return PARSE_ERROR_UNRECOGNISED_TVAL;

	sval = lookup_sval(tval, parser_getsym(p, "sval"));
	if (sval < 0)
		return PARSE_ERROR_UNRECOGNISED_SVAL;

	eopts = string_make(parser_getsym(p, "eopts"));
	einds = NULL;
	nind = 0;
	nalloc = 0;
	s = strtok(eopts, " |");
	while (s) {
		bool negated = false;
		int ind;

		if (prefix(s, "NOT-")) {
			negated = true;
			s += 4;
		}
		ind = lookup_option(s);
		if (ind > 0 && option_type(ind) == OP_BIRTH) {
			if (nind >= nalloc - 2) {
				if (nalloc == 0) {
					nalloc = 2;
				} else {
					nalloc *= 2;
				}
				einds = mem_realloc(einds,
					nalloc * sizeof(*einds));
			}
			einds[nind] = (negated) ? -ind : ind;
			einds[nind + 1] = 0;
			++nind;
		} else if (!streq(s, "none")) {
			mem_free(einds);
			string_free(eopts);
			return PARSE_ERROR_INVALID_OPTION;
		}
		s = strtok(NULL, " |");
	}
	string_free(eopts);

	si = mem_zalloc(sizeof *si);
	si->tval = tval;
	si->sval = sval;
	si->min = parser_getuint(p, "min");
	si->max = parser_getuint(p, "max");
	si->eopts = einds;

	if (si->min > 99 || si->max > 99) {
		mem_free(si->eopts);
		mem_free(si);
		return PARSE_ERROR_INVALID_ITEM_NUMBER;
	}

	si->next = c->start_items;
	c->start_items = si;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_obj_flags(struct parser *p) {
	struct player_class *c = parser_priv(p);
	char *flags;
	char *s;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(c->flags, OF_SIZE, list_obj_flag_names, s))
			break;
		s = strtok(NULL, " |");
	}

	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

static enum parser_error parse_class_play_flags(struct parser *p) {
	struct player_class *c = parser_priv(p);
	char *flags;
	char *s;

	if (!c)
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (!parser_hasval(p, "flags"))
		return PARSE_ERROR_NONE;
	flags = string_make(parser_getstr(p, "flags"));
	s = strtok(flags, " |");
	while (s) {
		if (grab_flag(c->pflags, PF_SIZE, player_info_flags, s))
			break;
		s = strtok(NULL, " |");
	}

	string_free(flags);
	return s ? PARSE_ERROR_INVALID_FLAG : PARSE_ERROR_NONE;
}

/**
 * Which realms a class may study in a given slot (PLR-08).
 *
 * Slot 1 or 2, and the two are not interchangeable: Zangband's entitlements are
 * asymmetric, so a Warrior-Mage's first realm is always Arcane and its second
 * is free. A class with only a slot 1 line studies one realm.
 *
 * Placed after the `magic:` line it governs, and refused before it, because a
 * class with nothing to cast has nothing to choose between -- which is why the
 * Monk and the Chaos-Warrior carry no entitlement yet despite Zangband giving
 * them one.
 */
static enum parser_error parse_class_realm_choice(struct parser *p) {
	struct player_class *c = parser_priv(p);
	unsigned int slot = parser_getuint(p, "slot");
	char *s, *t;

	if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;
	/*
	 * `books` is what `magic:` allocates; `num_books` counts up as the books
	 * themselves are read, so it is still zero on the line straight after the
	 * magic directive. Testing the wrong one of those two rejects every
	 * entitlement in the file.
	 */
	if (!c->magic.books) return PARSE_ERROR_MISSING_RECORD_HEADER;
	if (slot < 1 || slot > REALM_CHOICES) return PARSE_ERROR_INVALID_VALUE;

	s = string_make(parser_getstr(p, "realms"));
	t = strtok(s, " |");
	while (t) {
		const struct magic_realm *r = lookup_realm(t);

		if (!r) break;

		c->magic.realm_allowed[slot - 1][r->ridx] = true;
		t = strtok(NULL, " |");
	}
	string_free(s);

	if (t) return PARSE_ERROR_INVALID_VALUE;

	if ((int) slot > c->magic.realm_count) c->magic.realm_count = slot;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_magic(struct parser *p) {
	struct player_class *c = parser_priv(p);
	int num_books;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.books) {
		/* There's more than one magic directive for this class. */
		return PARSE_ERROR_REPEATED_DIRECTIVE;
	}
	c->magic.spell_first = parser_getuint(p, "first");
	c->magic.spell_weight = parser_getuint(p, "weight");
	num_books = parser_getuint(p, "books");
	c->magic.books = mem_zalloc(num_books * sizeof(struct class_book));
	class_max_books = num_books;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_book(struct parser *p) {
	struct player_class *c = parser_priv(p);
	int tval, spells;
	const char *name, *quality;
	struct class_book *b;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	tval = tval_find_idx(parser_getsym(p, "tval"));
	if (tval < 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	if (!c->magic.books || c->magic.num_books >= class_max_books) {
		/*
		 * This isn't the best description for the !c->magic.books
		 * case (no magic directive for the class before the book
		 * directive), but it's better than
		 * PARSE_ERROR_MISSING_RECORD_HEADER (already used above).
		 */
		return PARSE_ERROR_TOO_MANY_ENTRIES;
	}
	assert(c->magic.num_books >= 0);
	b = &c->magic.books[c->magic.num_books];
	b->tval = tval;

	quality = parser_getsym(p, "quality");
	if (streq(quality, "dungeon")) {
		b->dungeon = true;
	}
	name = parser_getsym(p, "name");
	write_book_kind(b, name);

	spells = parser_getuint(p, "spells");
	b->spells = mem_zalloc(spells * sizeof(struct class_spell));
	book_max_spells = spells;
	b->realm = lookup_realm(parser_getstr(p, "realm"));
	++c->magic.num_books;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_book_graphics(struct parser *p) {
	struct player_class *c = parser_priv(p);
	wchar_t glyph = parser_getchar(p, "glyph");
	const char *color = parser_getsym(p, "color");
	struct class_book *b;
	struct object_kind *k;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	b = &c->magic.books[c->magic.num_books - 1];
	k = lookup_kind(b->tval, b->sval);
	assert(k);
	k->d_char = glyph;
	if (strlen(color) > 1) {
		k->d_attr = color_text_to_attr(color);
	} else {
		k->d_attr = color_char_to_attr(color[0]);
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_book_properties(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *b;
	struct object_kind *k;
	const char *tmp;
	int amin, amax;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	b = &c->magic.books[c->magic.num_books - 1];
	k = lookup_kind(b->tval, b->sval);
	assert(k);
	k->cost = parser_getint(p, "cost");
	k->alloc_prob = parser_getint(p, "common");

	tmp = parser_getstr(p, "minmax");
	if (grab_int_range(&amin, &amax, tmp, "to")) {
		return PARSE_ERROR_INVALID_ALLOCATION;
	}
	k->level = amin;
	k->alloc_min = amin;
	k->alloc_max = amax;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_spell(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *book;
	struct class_spell *spell;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.  Use
		 * this under the assumption that without those, the maximum
		 * number of spells is zero.
		 */
		return PARSE_ERROR_TOO_MANY_ENTRIES;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	book = &c->magic.books[c->magic.num_books - 1];
	if (book->num_spells >= book_max_spells) {
		return PARSE_ERROR_TOO_MANY_ENTRIES;
	}
	assert(book->spells && book->num_spells >= 0);
	spell = &book->spells[book->num_spells];
	spell->realm = book->realm;
	spell->name = string_make(parser_getsym(p, "name"));
	spell->sidx = c->magic.total_spells;
	c->magic.total_spells++;
	spell->bidx = c->magic.num_books - 1;
	spell->slevel = parser_getint(p, "level");
	spell->smana = parser_getint(p, "mana");
	spell->sfail = parser_getint(p, "fail");
	spell->sexp = parser_getint(p, "exp");
	++book->num_spells;
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_effect(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *book;
	struct class_spell *spell;
	struct effect *effect, *new_effect;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	book = &c->magic.books[c->magic.num_books - 1];
	if (book->num_spells < 1) {
		/* Missing a spell directive after the book directive. */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(book->spells && book->num_spells <= book_max_spells);
	spell = &book->spells[book->num_spells - 1];
	/* Go to the next vacant effect and set it to the new one  */
	new_effect = mem_zalloc(sizeof(*effect));
	if (spell->effect) {
		effect = spell->effect;
		while (effect->next) effect = effect->next;
		effect->next = new_effect;
	} else {
		spell->effect = new_effect;
	}
	/* Fill in the detail */
	return grab_effect_data(p, new_effect);
}

static enum parser_error parse_class_effect_yx(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *book;
	struct class_spell *spell;
	struct effect *effect;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	book = &c->magic.books[c->magic.num_books - 1];
	if (book->num_spells < 1) {
		/* Missing a spell directive after the book directive. */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(book->spells && book->num_spells <= book_max_spells);
	spell = &book->spells[book->num_spells - 1];
	/* If there is no effect, assume that this is human and not parser error. */
	effect = spell->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;
	effect->y = parser_getint(p, "y");
	effect->x = parser_getint(p, "x");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_dice(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *book;
	struct class_spell *spell;
	struct effect *effect;
	dice_t *dice;
	const char *string;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	book = &c->magic.books[c->magic.num_books - 1];
	if (book->num_spells < 1) {
		/* Missing a spell directive after the book directive. */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(book->spells && book->num_spells <= book_max_spells);
	spell = &book->spells[book->num_spells - 1];
	/* If there is no effect, assume that this is human and not parser error. */
	effect = spell->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	dice = dice_new();
	if (dice == NULL) {
		return PARSE_ERROR_INVALID_DICE;
	}

	string = parser_getstr(p, "dice");
	if (dice_parse_string(dice, string)) {
		dice_free(effect->dice);
		effect->dice = dice;
	} else {
		dice_free(dice);
		return PARSE_ERROR_INVALID_DICE;
	}

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_expr(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *book;
	struct class_spell *spell;
	struct effect *effect;
	expression_t *expression;
	expression_base_value_f function;
	const char *name;
	const char *base;
	const char *expr;
	enum parser_error result;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	book = &c->magic.books[c->magic.num_books - 1];
	if (book->num_spells < 1) {
		/* Missing a spell directive after the book directive. */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(book->spells && book->num_spells <= book_max_spells);
	spell = &book->spells[book->num_spells - 1];
	/* If there is no effect, assume that this is human and not parser error. */
	effect = spell->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	/* If there are no dice, assume that this is human and not parser error. */
	if (effect->dice == NULL) {
		return PARSE_ERROR_NONE;
	}
	name = parser_getsym(p, "name");
	base = parser_getsym(p, "base");
	expr = parser_getstr(p, "expr");
	expression = expression_new();

	if (expression == NULL) {
		return PARSE_ERROR_INVALID_EXPRESSION;
	}
	function = effect_value_base_by_name(base);
	expression_set_base_value(expression, function);

	if (expression_add_operations_string(expression, expr) < 0) {
		result = PARSE_ERROR_BAD_EXPRESSION_STRING;
	} else if (dice_bind_expression(effect->dice, name, expression) < 0) {
		result = PARSE_ERROR_UNBOUND_EXPRESSION;
	} else {
		result = PARSE_ERROR_NONE;
	}

	/* The dice object makes a deep copy of the expression, so we can free it */
	expression_free(expression);

	return result;
}

static enum parser_error parse_class_effect_msg(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *book;
	struct class_spell *spell;
	struct effect *effect;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	book = &c->magic.books[c->magic.num_books - 1];
	if (book->num_spells < 1) {
		/* Missing a spell directive after the book directive. */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(book->spells && book->num_spells <= book_max_spells);
	spell = &book->spells[book->num_spells - 1];
	/* If there is no effect, assume that this is human and not parser error. */
	effect = spell->effect;
	if (effect == NULL) {
		return PARSE_ERROR_NONE;
	}
	while (effect->next) effect = effect->next;

	effect->msg = string_append(effect->msg, parser_getstr(p, "text"));
	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_desc(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_book *book;
	struct class_spell *spell;

	if (!c) {
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	if (c->magic.num_books < 1) {
		/*
		 * Either missing a magic directive for the class or didn't
		 * have a book directive after the magic directive.
		 */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(c->magic.books && c->magic.num_books <= class_max_books);
	book = &c->magic.books[c->magic.num_books - 1];
	if (book->num_spells < 1) {
		/* Missing a spell directive after the book directive. */
		return PARSE_ERROR_MISSING_RECORD_HEADER;
	}
	assert(book->spells && book->num_spells <= book_max_spells);
	spell = &book->spells[book->num_spells - 1];
	spell->text = string_append(spell->text, parser_getstr(p, "desc"));
	return PARSE_ERROR_NONE;
}

/**
 * One rung of a class's unarmed ladder (ZangbandTK, PLR-04).
 *
 *	blow:<level>:<chance>:<dice>:<effect>:<power>:<message>
 *
 * The message takes one %s, which is the target.  Effect is NONE, KNEE, SLOW or
 * STUN, and power is the stun magnitude STUN uses and the others ignore.
 */
static enum parser_error parse_class_blow(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct class_blow *blow, *last;
	const char *effect;
	const char *dice;

	if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

	blow = mem_zalloc(sizeof(*blow));
	blow->level = parser_getint(p, "level");
	blow->chance = parser_getint(p, "chance");
	blow->power = parser_getint(p, "power");

	dice = parser_getsym(p, "dice");
	if (sscanf(dice, "%dd%d", &blow->dd, &blow->ds) != 2 ||
			blow->dd < 1 || blow->ds < 1) {
		mem_free(blow);
		return PARSE_ERROR_INVALID_DICE;
	}

	effect = parser_getsym(p, "effect");
	if (streq(effect, "NONE")) {
		blow->effect = MA_NONE;
	} else if (streq(effect, "KNEE")) {
		blow->effect = MA_KNEE;
	} else if (streq(effect, "SLOW")) {
		blow->effect = MA_SLOW;
	} else if (streq(effect, "STUN")) {
		blow->effect = MA_STUN;
	} else {
		mem_free(blow);
		return PARSE_ERROR_INVALID_VALUE;
	}

	/*
	 * The message is handed to strnfmt() with the target's name, so it has to
	 * be exactly one %s and nothing else.  Checked here rather than trusted,
	 * because a slip in the data file would otherwise be a crash in melee.
	 */
	{
		const char *desc = parser_getstr(p, "desc");
		const char *s;
		int subs = 0;

		for (s = desc; *s; s++) {
			if (*s != '%') continue;
			if (s[1] == '%') { s++; continue; }
			if (s[1] != 's') { mem_free(blow); return PARSE_ERROR_INVALID_VALUE; }
			subs++;
			s++;
		}

		if (subs != 1) {
			mem_free(blow);
			return PARSE_ERROR_INVALID_VALUE;
		}

		blow->desc = string_make(desc);
	}

	/* Kept in file order, which is the order they are written in: easiest. */
	if (c->blows) {
		for (last = c->blows; last->next; last = last->next)
			;
		last->next = blow;
	} else {
		c->blows = blow;
	}

	return PARSE_ERROR_NONE;
}

/* ---- the class's copies (PLR-06) ---- */

/**
 * `powers:<stat>:<first level>` says the class carries a power list instead of
 * spellbooks, and where the mana for it comes from.
 *
 * calc_mana() reads the casting stat out of the realms a class's books belong
 * to, and a class with no books has no realms, so a power-list class has to
 * name its stat itself or it would have nothing to spend.
 */
static enum parser_error parse_class_powers(struct parser *p) {
	struct player_class *c = parser_priv(p);
	int stat;

	if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

	stat = stat_name_to_idx(parser_getsym(p, "stat"));
	if (stat < 0) return PARSE_ERROR_INVALID_VALUE;

	c->power_stat = stat;
	c->power_first = parser_getint(p, "first");
	c->power_weight = parser_getint(p, "weight");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_power(struct parser *p) {
	struct player_class *c = parser_priv(p);

	if (!c) return PARSE_ERROR_MISSING_RECORD_HEADER;

	return power_parse_new(&c->powers, parser_getstr(p, "name"));
}

static enum parser_error parse_class_power_level(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct player_power *power = c ? power_last(c->powers) : NULL;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	power->level = parser_getint(p, "level");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_power_cost(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct player_power *power = c ? power_last(c->powers) : NULL;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	power->cost = parser_getint(p, "cost");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_power_fail(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct player_power *power = c ? power_last(c->powers) : NULL;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;
	power->fail = parser_getint(p, "fail");

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_power_stat(struct parser *p) {
	struct player_class *c = parser_priv(p);
	struct player_power *power = c ? power_last(c->powers) : NULL;
	int stat;

	if (!power) return PARSE_ERROR_MISSING_RECORD_HEADER;

	stat = stat_name_to_idx(parser_getsym(p, "stat"));
	if (stat < 0) return PARSE_ERROR_INVALID_VALUE;
	power->stat = stat;

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_class_power_when(struct parser *p) {
	struct player_class *c = parser_priv(p);

	return power_parse_when(c ? power_last(c->powers) : NULL,
							parser_getint(p, "from"), parser_getint(p, "to"));
}

static enum parser_error parse_class_power_effect(struct parser *p) {
	struct player_class *c = parser_priv(p);

	return power_parse_effect(c ? power_last(c->powers) : NULL, p);
}

static enum parser_error parse_class_power_dice(struct parser *p) {
	struct player_class *c = parser_priv(p);

	return power_parse_dice(c ? power_last(c->powers) : NULL,
							parser_getstr(p, "dice"));
}

static enum parser_error parse_class_power_expr(struct parser *p) {
	struct player_class *c = parser_priv(p);

	return power_parse_expr(c ? power_last(c->powers) : NULL,
							parser_getsym(p, "name"), parser_getsym(p, "base"),
							parser_getstr(p, "expr"));
}

static struct parser *init_parse_class(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);
	parser_reg(p, "name str name", parse_class_name);
	parser_reg(p, "virtues str virtues", parse_class_virtues);
	parser_reg(p, "stats int str int int int wis int dex int con",
			   parse_class_stats);
	parser_reg(p, "skill-disarm-phys int base int incr",
			   parse_class_skill_disarm_phys);
	parser_reg(p, "skill-disarm-magic int base int incr",
			   parse_class_skill_disarm_magic);
	parser_reg(p, "skill-device int base int incr", parse_class_skill_device);
	parser_reg(p, "skill-save int base int incr", parse_class_skill_save);
	parser_reg(p, "skill-stealth int base int incr", parse_class_skill_stealth);
	parser_reg(p, "skill-search int base int incr", parse_class_skill_search);
	parser_reg(p, "skill-melee int base int incr", parse_class_skill_melee);
	parser_reg(p, "skill-shoot int base int incr", parse_class_skill_shoot);
	parser_reg(p, "skill-throw int base int incr", parse_class_skill_throw);
	parser_reg(p, "skill-dig int base int incr", parse_class_skill_dig);
	parser_reg(p, "hitdie int mhp", parse_class_hitdie);
	parser_reg(p, "exp int exp", parse_class_exp);
	parser_reg(p, "max-attacks int max-attacks", parse_class_max_attacks);
	parser_reg(p, "pet-upkeep-div int pet-upkeep-div", parse_class_pet_upkeep);
	parser_reg(p, "min-weight int min-weight", parse_class_min_weight);
	parser_reg(p, "strength-multiplier int att-multiply", parse_class_str_mult);
	parser_reg(p, "title str title", parse_class_title);
	parser_reg(p, "equip sym tval sym sval uint min uint max sym eopts",
			   parse_class_equip);
	parser_reg(p, "obj-flags ?str flags", parse_class_obj_flags);
	parser_reg(p, "player-flags ?str flags", parse_class_play_flags);
	parser_reg(p, "magic uint first uint weight uint books", parse_class_magic);
	parser_reg(p, "realm-choice uint slot str realms",
			   parse_class_realm_choice);
	parser_reg(p, "book sym tval sym quality sym name uint spells str realm",
			   parse_class_book);
	parser_reg(p, "book-graphics char glyph sym color",
			   parse_class_book_graphics);
	parser_reg(p, "book-properties int cost int common str minmax",
			   parse_class_book_properties);
	parser_reg(p, "spell sym name int level int mana int fail int exp",
			   parse_class_spell);
	parser_reg(p, "effect sym eff ?sym type ?int radius ?int other", parse_class_effect);
	parser_reg(p, "effect-yx int y int x", parse_class_effect_yx);
	parser_reg(p, "dice str dice", parse_class_dice);
	parser_reg(p, "expr sym name sym base str expr", parse_class_expr);
	parser_reg(p, "effect-msg str text", parse_class_effect_msg);
	parser_reg(p, "desc str desc", parse_class_desc);
	parser_reg(p, "blow int level int chance sym dice sym effect int power "
			   "str desc", parse_class_blow);
	parser_reg(p, "powers sym stat int first int weight",
			   parse_class_powers);
	parser_reg(p, "power str name", parse_class_power);
	parser_reg(p, "power-level int level", parse_class_power_level);
	parser_reg(p, "power-cost int cost", parse_class_power_cost);
	parser_reg(p, "power-stat sym stat", parse_class_power_stat);
	parser_reg(p, "power-fail int fail", parse_class_power_fail);
	parser_reg(p, "power-when int from int to", parse_class_power_when);
	parser_reg(p, "power-effect sym eff ?sym type ?int radius ?int other",
			   parse_class_power_effect);
	parser_reg(p, "power-dice str dice", parse_class_power_dice);
	parser_reg(p, "power-expr sym name sym base str expr",
			   parse_class_power_expr);
	return p;
}

static errr run_parse_class(struct parser *p) {
	return parse_file_quit_not_found(p, "class");
}

static errr finish_parse_class(struct parser *p) {
	struct player_class *c;
	int num = 0;
	classes = parser_priv(p);
	for (c = classes; c; c = c->next) num++;
	for (c = classes; c; c = c->next, num--) {
		assert(num);
		c->cidx = num - 1;
	}
	parser_destroy(p);
	return 0;
}

static void cleanup_class(void)
{
	struct player_class *c = classes;
	struct player_class *next;
	struct start_item *item, *item_next;
	struct class_spell *spell;
	struct class_book *book;
	int i, j;

	while (c) {
		struct class_blow *blow = c->blows;

		next = c->next;

		/* And its power list, if it carries one (PLR-06). */
		power_free(c->powers);

		/* And the unarmed ladder, if the class has one (PLR-04). */
		while (blow) {
			struct class_blow *bnext = blow->next;

			string_free(blow->desc);
			mem_free(blow);
			blow = bnext;
		}

		item = c->start_items;
		while(item) {
			item_next = item->next;
			mem_free(item->eopts);
			mem_free(item);
			item = item_next;
		}
		for (i = 0; i < c->magic.num_books; i++) {
			book = &c->magic.books[i];
			for (j = 0; j < book->num_spells; j++) {
				spell = &book->spells[j];
				string_free(spell->name);
				string_free(spell->text);
				free_effect(spell->effect);
			}
			mem_free(book->spells);
		}
		mem_free(c->magic.books);
		for (i = (int) N_ELEMENTS(c->title) - 1; i >= 0; --i) {
			string_free((char *)c->title[i]);
		}
		string_free((char *)c->name);
		mem_free(c);
		c = next;
	}
}

struct file_parser class_parser = {
	"class",
	init_parse_class,
	run_parse_class,
	finish_parse_class,
	cleanup_class
};

/**
 * ------------------------------------------------------------------------
 * Initialize flavors
 * ------------------------------------------------------------------------ */

static wchar_t flavor_glyph;
static unsigned int flavor_tval;

static enum parser_error parse_flavor_flavor(struct parser *p) {
	struct flavor *h = parser_priv(p);
	struct flavor *f = mem_zalloc(sizeof *f);

	const char *attr;
	int d_attr;

	f->next = h;

	f->fidx = parser_getuint(p, "index");
	f->tval = flavor_tval;
	f->d_char = flavor_glyph;

	if (parser_hasval(p, "sval"))
		f->sval = lookup_sval(f->tval, parser_getsym(p, "sval"));
	else
		f->sval = SV_UNKNOWN;

	attr = parser_getsym(p, "attr");
	if (strlen(attr) == 1)
		d_attr = color_char_to_attr(attr[0]);
	else
		d_attr = color_text_to_attr(attr);

	if (d_attr < 0)
		return PARSE_ERROR_INVALID_COLOR;
	f->d_attr = d_attr;

	if (parser_hasval(p, "desc"))
		f->text = string_append(f->text, parser_getstr(p, "desc"));

	parser_setpriv(p, f);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_flavor_kind(struct parser *p) {
	int tval = tval_find_idx(parser_getsym(p, "tval"));

	if (tval <= 0) {
		return PARSE_ERROR_UNRECOGNISED_TVAL;
	}
	flavor_glyph = parser_getchar(p, "glyph");
	flavor_tval = (unsigned int) tval;
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_flavor(void) {
	struct parser *p = parser_new();
	parser_setpriv(p, NULL);

	parser_reg(p, "kind sym tval char glyph", parse_flavor_kind);
	parser_reg(p, "flavor uint index sym attr ?str desc", parse_flavor_flavor);
	parser_reg(p, "fixed uint index sym sval sym attr ?str desc", parse_flavor_flavor);

	return p;
}

static errr run_parse_flavor(struct parser *p) {
	errr err = parse_file_quit_not_found(p, "flavor");

	if (err)
		return err;

	/*
	 * ZangbandTK (CNT-11): the extra ring and amulet flavours the imported
	 * kinds need.  4.2 gives every kind of a flavoured tval its own flavour
	 * and quits if the pool runs dry, so importing fifteen rings into a
	 * game with thirty-nine ring flavours for thirty rings needs more.
	 */
	err = parse_file(p, "flavor.zangband");

	return (err == PARSE_ERROR_NO_FILE_FOUND) ? PARSE_ERROR_NONE : err;
}

static errr finish_parse_flavor(struct parser *p) {
	flavors = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_flavor(void)
{
	struct flavor *f, *next;

	f = flavors;
	while(f) {
		next = f->next;
		/* Hack - scrolls get randomly-generated names */
		if (f->tval != TV_SCROLL)
			mem_free(f->text);
		mem_free(f);
		f = next;
	}
}

struct file_parser flavor_parser = {
	"flavor",
	init_parse_flavor,
	run_parse_flavor,
	finish_parse_flavor,
	cleanup_flavor
};


/**
 * ------------------------------------------------------------------------
 * Initialize hints
 * ------------------------------------------------------------------------ */

static enum parser_error parse_hint(struct parser *p) {
	struct hint *h = parser_priv(p);
	struct hint *new = mem_zalloc(sizeof *new);

	new->hint = string_make(parser_getstr(p, "text"));
	new->next = h;

	parser_setpriv(p, new);
	return PARSE_ERROR_NONE;
}

static struct parser *init_parse_hints(void) {
	struct parser *p = parser_new();
	parser_reg(p, "H str text", parse_hint);
	return p;
}

static errr run_parse_hints(struct parser *p) {
	return parse_file_quit_not_found(p, "hints");
}

static errr finish_parse_hints(struct parser *p) {
	hints = parser_priv(p);
	parser_destroy(p);
	return 0;
}

static void cleanup_hints(void)
{
	struct hint *h, *next;

	h = hints;
	while(h) {
		next = h->next;
		string_free(h->hint);
		mem_free(h);
		h = next;
	}
}

struct file_parser hints_parser = {
	"hints",
	init_parse_hints,
	run_parse_hints,
	finish_parse_hints,
	cleanup_hints
};

/**
 * ------------------------------------------------------------------------
 * Initialize what a monster says (ZangbandTK, CNT-04)
 * ------------------------------------------------------------------------ */

struct monster_speech mon_speech;

static enum parser_error parse_speech_line(struct parser *p, const char *key)
{
	struct monster_speech_pool *pool = monster_speech_pool(&mon_speech, key);
	const char *text = parser_getstr(p, "text");

	if (!pool) return PARSE_ERROR_INTERNAL;

	pool->line = mem_realloc(pool->line, (pool->count + 1) * sizeof(*pool->line));
	pool->line[pool->count++] = string_make(text);

	return PARSE_ERROR_NONE;
}

static enum parser_error parse_speech_speak(struct parser *p) {
	return parse_speech_line(p, "speak");
}

static enum parser_error parse_speech_fear(struct parser *p) {
	return parse_speech_line(p, "fear");
}

static enum parser_error parse_speech_death(struct parser *p) {
	return parse_speech_line(p, "death");
}

static enum parser_error parse_speech_crime(struct parser *p) {
	return parse_speech_line(p, "crime");
}

static struct parser *init_parse_speech(void) {
	struct parser *p = parser_new();
	parser_reg(p, "speak str text", parse_speech_speak);
	parser_reg(p, "fear str text", parse_speech_fear);
	parser_reg(p, "death str text", parse_speech_death);
	parser_reg(p, "crime str text", parse_speech_crime);
	return p;
}

static errr run_parse_speech(struct parser *p) {
	return parse_file_quit_not_found(p, "monster_speech");
}

static errr finish_parse_speech(struct parser *p) {
	parser_destroy(p);
	return 0;
}

static void cleanup_speech(void)
{
	monster_speech_free(&mon_speech);
}

struct file_parser speech_parser = {
	"monster_speech",
	init_parse_speech,
	run_parse_speech,
	finish_parse_speech,
	cleanup_speech
};

/**
 * ------------------------------------------------------------------------
 * Game data initialization
 * ------------------------------------------------------------------------ */

/**
 * A list of all the above parsers, plus those found in mon-init.c and
 * obj-init.c
 */
static struct {
	const char *name;
	struct file_parser *parser;
} pl[] = {
	{ "world", &world_parser },
	{ "projections", &projection_parser },
	{ "ui renderers", &ui_entry_renderer_parser },
	{ "ui entries", &ui_entry_parser },
	{ "player properties", &player_property_parser },
	{ "features", &feat_parser },
	{ "object bases", &object_base_parser },
	{ "slays", &slay_parser },
	{ "brands", &brand_parser },
	{ "monster pain messages", &pain_parser },
	{ "monster bases", &mon_base_parser },
	{ "summons", &summon_parser },
	{ "curses", &curse_parser },
	{ "player shapes", &shape_parser },
	{ "objects", &object_parser },
	{ "activations", &act_parser },
	{ "ego-items", &ego_parser },
	{ "history charts", &history_parser },
	{ "bodies", &body_parser },
	{ "player races", &p_race_parser },
	{ "renames", &rename_parser },
	{ "magic realms", &realm_parser },
	{ "player classes", &class_parser },
	{ "artifacts", &artifact_parser },
	{ "object properties", &object_property_parser },
	{ "timed effects", &player_timed_parser },
	{ "blow methods", &meth_parser },
	{ "blow effects", &eff_parser },
	{ "monster spells", &mon_spell_parser },
	{ "monsters", &monster_parser },
	{ "dungeons", &dungeon_parser },
	{ "town names", &town_parser },
	{ "shop quality", &quality_parser },
	{ "chaos patrons", &patron_parser },
	{ "mutations", &mutation_parser },
	{ "monster pits" , &pit_parser },
	{ "monster lore" , &lore_parser },
	{ "traps", &trap_parser },
	{ "chest_traps", &chest_trap_parser },
	{ "quests", &quests_parser },
	{ "flavours", &flavor_parser },
	{ "hints", &hints_parser },
	{ "monster speech", &speech_parser },
	{ "random names", &names_parser }
};

/**
 * Initialize just the internal arrays.
 * This should be callable by the test suite, without relying on input, or
 * anything to do with a user or savefiles.
 *
 * Assumption: Paths are set up correctly before calling this function.
 */
void init_arrays(void)
{
	unsigned int i;

	for (i = 0; i < N_ELEMENTS(pl); i++) {
		char *msg = string_make(format("Initializing %s...", pl[i].name));
		event_signal_message(EVENT_INITSTATUS, 0, msg);
		string_free(msg);
		if (run_parser(pl[i].parser))
			quit_fmt("Cannot initialize %s.", pl[i].name);
	}
}

/**
 * Free all the internal arrays
 */
static void cleanup_arrays(void)
{
	unsigned int i;

	for (i = 1; i < N_ELEMENTS(pl); i++)
		cleanup_parser(pl[i].parser);

	cleanup_parser(pl[0].parser);
}

static struct init_module arrays_module = {
	.name = "arrays",
	.init = init_arrays,
	.cleanup = cleanup_arrays
};


extern struct init_module z_quark_module;
extern struct init_module generate_module;
extern struct init_module rune_module;
extern struct init_module obj_make_module;
extern struct init_module ignore_module;
extern struct init_module mon_make_module;
extern struct init_module player_module;
extern struct init_module store_module;
extern struct init_module messages_module;
extern struct init_module options_module;
extern struct init_module ui_player_module;
extern struct init_module ui_equip_cmp_module;

static struct init_module *modules[] = {
	&z_quark_module,
	&messages_module,
	&ui_visuals_module, /* This needs to load before monsters and objects. */
	&arrays_module,
	&player_module,
	&generate_module,
	&rune_module,
	&obj_make_module,
	&ignore_module,
	&mon_make_module,
	&store_module,
	&options_module,
	&ui_player_module,
	&ui_equip_cmp_module,
	NULL
};

/**
 * Initialise Angband's data stores and allocate memory for structures,
 * etc, so that the game can get started.
 *
 * The only input/output in this file should be via event_signal_string().
 * We cannot rely on any particular UI as this part should be UI-agnostic.
 * We also cannot rely on anything else having being initialised into any
 * particular state.  Which is why you'd be calling this function in the
 * first place.
 *
 * Old comment, not sure if still accurate:
 * Note that the "graf-xxx.prf" file must be loaded separately,
 * if needed, in the first (?) pass through "TERM_XTRA_REACT".
 */
bool init_angband(void)
{
	int i;

	event_signal(EVENT_ENTER_INIT);

	init_game_constants();

	/* Initialise modules */
	for (i = 0; modules[i]; i++)
		if (modules[i]->init)
			modules[i]->init();

	/* Initialize some other things */
	event_signal_message(EVENT_INITSTATUS, 0, "Initializing other stuff...");

	/* List display codes */
	monster_list_init();
	object_list_init();

	/* Initialise RNG */
	event_signal_message(EVENT_INITSTATUS, 0, "Getting the dice rolling...");
	Rand_init();

	return true;
}

/**
 * Free all the stuff initialised in init_angband()
 */
void cleanup_angband(void)
{
	int i;

	/*
	 * Anything a monster was about to say goes with the monsters (ZangbandTK).
	 * The stacked messages hold race pointers, and the races are freed a few
	 * lines below; leaving them stacked leaves a dangling read for whatever
	 * flushes the queue next, which for a second character in one process is
	 * a real one.
	 */
	monster_messages_reset();

	/*
	 * And the pets in transit, if the game is torn down mid-transition
	 * (ZangbandTK, PLR-26).  The buffer is reused across level changes rather
	 * than allocated each time, so something has to give it back.
	 */
	pets_in_transit_free();

	/* Free the chunk list */
	for (i = 0; i < chunk_list_max; i++) {
		wipe_mon_list(chunk_list[i], player);
		cave_free(chunk_list[i]);
	}
	mem_free(chunk_list);
	chunk_list = NULL;

	for (i = 0; modules[i]; i++)
		if (modules[i]->cleanup)
			modules[i]->cleanup();

	event_remove_all_handlers();

	/* Free the main cave */
	if (cave) {
		cave_free(cave);
		cave = NULL;
		character_dungeon = false;
	}

	/* Free the world the cave sits under (ZangbandTK) */
	wild_cleanup();

	monster_list_finalize();
	object_list_finalize();

	cleanup_game_constants();

	cmdq_release();

	if (play_again) return;

	/* Free the format() buffer */
	vformat_kill();

	/*
	 * Free the directories, and clear them.
	 *
	 * Upstream frees without clearing, which is safe only while nothing ever
	 * initialises a second time in one process. `init_file_paths()` opens by
	 * freeing whatever is there, so after a cleanup that left these dangling
	 * the next call double-frees and libmalloc aborts -- with a backtrace that
	 * unwinds to nothing useful, which is how this stayed hidden. It is why a
	 * unit test could not build a character after resetting the game, and why
	 * the only reset in the suites is the one that sets `play_again` and so
	 * never reaches this block at all.
	 */
	string_free(ANGBAND_DIR_GAMEDATA);
	ANGBAND_DIR_GAMEDATA = NULL;
	string_free(ANGBAND_DIR_CUSTOMIZE);
	ANGBAND_DIR_CUSTOMIZE = NULL;
	string_free(ANGBAND_DIR_HELP);
	ANGBAND_DIR_HELP = NULL;
	string_free(ANGBAND_DIR_SCREENS);
	ANGBAND_DIR_SCREENS = NULL;
	string_free(ANGBAND_DIR_FONTS);
	ANGBAND_DIR_FONTS = NULL;
	string_free(ANGBAND_DIR_TILES);
	ANGBAND_DIR_TILES = NULL;
	string_free(ANGBAND_DIR_SOUNDS);
	ANGBAND_DIR_SOUNDS = NULL;
	string_free(ANGBAND_DIR_ICONS);
	ANGBAND_DIR_ICONS = NULL;
	string_free(ANGBAND_DIR_USER);
	ANGBAND_DIR_USER = NULL;
	string_free(ANGBAND_DIR_SAVE);
	ANGBAND_DIR_SAVE = NULL;
	string_free(ANGBAND_DIR_PANIC);
	ANGBAND_DIR_PANIC = NULL;
	string_free(ANGBAND_DIR_SCORES);
	ANGBAND_DIR_SCORES = NULL;
	string_free(ANGBAND_DIR_ARCHIVE);
	ANGBAND_DIR_ARCHIVE = NULL;
}
