/**
 * \file obj-util.c
 * \brief Object utilities
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
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

#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "effects.h"
#include "game-input.h"
#include "game-world.h"
#include "generate.h"
#include "grafmode.h"
#include "init.h"
#include "mon-make.h"
#include "monster.h"
#include "obj-curse.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-slays.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-history.h"
#include "player-spell.h"
#include "player-util.h"
#include "randname.h"
#include "z-queue.h"

struct object_base *kb_info;
struct object_kind *k_info;
struct artifact *a_info;
struct artifact_upkeep *aup_info;
struct ego_item *e_info;
struct flavor *flavors;

/**
 * Hold the titles of scrolls, 6 to 14 characters each, plus quotes.
 */
static char scroll_adj[MAX_TITLES][18];

static void flavor_assign_fixed(void)
{
	int i;
	struct flavor *f;

	for (f = flavors; f; f = f->next) {
		if (f->sval == SV_UNKNOWN)
			continue;

		for (i = 0; i < z_info->k_max; i++) {
			struct object_kind *k = &k_info[i];
			if (k->tval == f->tval && k->sval == f->sval)
				k->flavor = f;
		}
	}
}


static void flavor_assign_random(uint8_t tval)
{
	int i;
	int flavor_count = 0;
	int choice;
	struct flavor *f;

	/* Count the random flavors for the given tval */
	for (f = flavors; f; f = f->next)
		if (f->tval == tval && f->sval == SV_UNKNOWN)
			flavor_count++;

	for (i = 0; i < z_info->k_max; i++) {
		if (k_info[i].tval != tval || k_info[i].flavor)
			continue;

		if (!flavor_count)
			quit_fmt("Not enough flavors for tval %d.", tval);

		choice = randint0(flavor_count);
	
		for (f = flavors; f; f = f->next) {
			if (f->tval != tval || f->sval != SV_UNKNOWN)
				continue;

			if (choice == 0) {
				k_info[i].flavor = f;
				f->sval = k_info[i].sval;
				if (tval == TV_SCROLL)
					f->text = scroll_adj[k_info[i].sval];
				flavor_count--;
				break;
			}

			choice--;
		}
	}
}

/**
 * Reset svals on flavors, effectively removing any fixed flavors.
 *
 * Mainly useful for randarts so that fixed flavors for standards aren't
 * predictable. The One Ring is kept as fixed, since it lives through randarts.
 */
static void flavor_reset_fixed(void)
{
	struct flavor *f;

	for (f = flavors; f; f = f->next) {
		if (f->tval == TV_RING && strstr(f->text, "Plain Gold"))
			continue;

		f->sval = SV_UNKNOWN;
	}
}

/**
 * Prepare the "variable" part of the "k_info" array.
 *
 * The "color"/"metal"/"type" of an item is its "flavor".
 * For the most part, flavors are assigned randomly each game.
 *
 * Initialize descriptions for the "colored" objects, including:
 * Rings, Amulets, Staffs, Wands, Rods, Mushrooms, Potions, Scrolls.
 *
 * Scroll titles are always between 6 and 14 letters long.  This is
 * ensured because every title is composed of whole words, where every
 * word is from 2 to 8 letters long, and that no scroll is finished
 * until it attempts to grow beyond 15 letters.  The first time this
 * can happen is when the current title has 6 letters and the new word
 * has 8 letters, which would result in a 6 letter scroll title.
 *
 * Make sure everything stays the same for each saved game
 * This is accomplished by the use of a saved "random seed", as in
 * "town_gen()".  Since no other functions are called while the special
 * seed is in effect, so this function is pretty "safe".
 */
void flavor_init(void)
{
	int i, j;

	/* Use the "simple" RNG */
	Rand_quick = true;

	/* Induce consistent flavors */
	Rand_value = seed_flavor;

	/* Scrub all flavors and re-parse for new players */
	if (turn == 1) {
		struct flavor *f;

		for (i = 0; i < z_info->k_max; i++) {
			k_info[i].flavor = NULL;
		}
		for (f = flavors; f; f = f->next) {
			f->sval = SV_UNKNOWN;
		}
		cleanup_parser(&flavor_parser);
		if (run_parser(&flavor_parser)) {
			quit("Could not parse flavor.txt.");
		}
	}

	if (OPT(player, birth_randarts))
		flavor_reset_fixed();

	flavor_assign_fixed();

	flavor_assign_random(TV_RING);
	flavor_assign_random(TV_AMULET);
	flavor_assign_random(TV_STAFF);
	flavor_assign_random(TV_WAND);
	flavor_assign_random(TV_ROD);
	flavor_assign_random(TV_MUSHROOM);
	flavor_assign_random(TV_POTION);

	/* Scrolls (random titles, always white) */
	for (i = 0; i < MAX_TITLES; i++) {
		char buf[26];
		char *end = buf + 1;
		int titlelen = 0;
		int wordlen;
		bool okay = true;

		my_strcpy(buf, "\"", 2);
		wordlen = randname_make(RANDNAME_SCROLL, 2, 8, end, 24, name_sections);
		while (titlelen + wordlen < (int)(sizeof(scroll_adj[0]) - 3)) {
			end[wordlen] = ' ';
			titlelen += wordlen + 1;
			end += wordlen + 1;
			wordlen = randname_make(RANDNAME_SCROLL, 2, 8, end, 24 - titlelen,
									name_sections);
		}
		buf[titlelen] = '"';
		buf[titlelen+1] = '\0';

		/* Check the scroll name hasn't already been generated */
		for (j = 0; j < i; j++) {
			if (streq(buf, scroll_adj[j])) {
				okay = false;
				break;
			}
		}

		if (okay)
			my_strcpy(scroll_adj[i], buf, sizeof(scroll_adj[0]));
		else
			/* Have another go at making a name */
			i--;
	}
	flavor_assign_random(TV_SCROLL);

	/* Use the "complex" RNG */
	Rand_quick = false;

	/* Analyze every object */
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Skip "empty" objects */
		if (!kind->name) continue;

		/*
		 * No flavor and not kind that only has one instance,
		 * an artifact, yields aware
		 */
		if (!kind->flavor && kind->kidx < z_info->ordinary_kind_max) {
			kind->aware = true;
		}
	}
}

/**
 * Set all flavors as aware
 */
void flavor_set_all_aware(void)
{
	int i;

	/* Analyze every object */
	for (i = 0; i < z_info->k_max; i++) {
		struct object_kind *kind = &k_info[i];

		/* Skip empty objects */
		if (!kind->name) continue;

		/* Flavor yields aware */
		if (kind->flavor) kind->aware = true;
	}
}

/**
 * Return the weight, in 1/10ths of pounds and including curses, of one object
 * from a stack.
 *
 * obj->weight is only the base weight and does not include curses.
 * Modifications to the weight from curses will not cause the weight to
 * fall outside of the range of [0, 32767].
 */
int16_t object_weight_one(const struct object *obj)
{
	int16_t result = MAX(obj->weight, 0);

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result = modify_weight_for_curse(i, result);
			}
		}
	}

	return result;
}

/**
 * Return the hit bonus for an object, including any of its curses.
 */
int object_to_hit(const struct object *obj)
{
	int result = obj->to_h;

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result += curses[i].obj->to_h;
			}
		}
	}
	return result;
}

/**
 * Return the damage bonus for an object, including any of its curses.
 */
int object_to_dam(const struct object *obj)
{
	int result = obj->to_d;

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result += curses[i].obj->to_d;
			}
		}
	}
	return result;
}

/**
 * Return the armor class bonus for an object, including any of its curses.
 */
int object_to_ac(const struct object *obj)
{
	int result = obj->to_a;

	if (obj->curses) {
		int i;

		for (i = 1; i < z_info->curse_max; ++i) {
			if (obj->curses[i].power) {
				result += curses[i].obj->to_a;
			}
		}
	}
	return result;
}

/**
 * Obtain the flags for an item
 */
void object_flags(const struct object *obj, bitflag flags[OF_SIZE])
{
	of_wipe(flags);
	if (!obj) return;
	of_copy(flags, obj->flags);
}


/**
 * Obtain the flags for an item which are known to the player
 */
void object_flags_known(const struct object *obj, bitflag flags[OF_SIZE])
{
	object_flags(obj, flags);
	of_inter(flags, obj->known->flags);

	if (!obj->kind) {
		return;
	}

	if (object_flavor_is_aware(obj)) {
		of_union(flags, obj->kind->flags);
	}

	if (obj->ego && easy_know(obj)) {
		of_union(flags, obj->ego->flags);
		of_diff(flags, obj->ego->flags_off);
	}
}

/**
 * Apply a tester function, skipping all non-objects and gold
 */
bool object_test(item_tester tester, const struct object *obj)
{
	/* Require object */
	if (!obj) return false;

	/* Ignore gold */
	if (tval_is_money(obj)) return false;

	/* Pass without a tester, or tail-call the tester if it exists */
	return !tester || tester(obj);
}


/**
 * Return true if the item is unknown (has yet to be seen by the player).
 */
bool is_unknown(const struct object *obj)
{
	struct grid_data gd = {
		.m_idx = 0,
		.f_idx = 0,
		.first_kind = NULL,
		.trap = NULL,
		.multiple_objects = false,
		.unseen_object = false,
		.unseen_money = false,
		.lighting = LIGHTING_LOS,
		.in_view = false,
		.is_player = false,
		.hallucinate = false,
	};
	map_info(obj->grid, &gd);
	return gd.unseen_object;
}	


/**
 * Looks if "inscrip" is present on the given object.
 */
unsigned check_for_inscrip(const struct object *obj, const char *inscrip)
{
	unsigned i = 0;
	const char *s;

	if (!obj->note) return 0;

	s = quark_str(obj->note);

	/* Needing this implies there are bad instances of obj->note around,
	 * but I haven't been able to track down their origins - NRM */
	if (!s) return 0;

	do {
		s = strstr(s, inscrip);
		if (!s) break;

		i++;
		s++;
	} while (s);

	return i;
}

/**
 * Looks if "inscrip" immediately followed by a decimal integer without a
 * leading sign character is present on the given object.  Returns the number
 * of times such an inscription occurs and, if that value is at least one,
 * sets *ival to the value of the integer that followed the first such
 * inscription.
 */
unsigned check_for_inscrip_with_int(const struct object *obj, const char *inscrip, int* ival)
{
	unsigned i = 0;
	size_t inlen = strlen(inscrip);
	const char *s;

	if (!obj->note) return 0;

	s = quark_str(obj->note);

	/* Needing this implies there are bad instances of obj->note around,
	 * but I haven't been able to track down their origins - NRM */
	if (!s) return 0;

	do {
		s = strstr(s, inscrip);
		if (!s) break;
		if (isdigit((unsigned char)s[inlen])) {
			if (i == 0) {
				long inarg = strtol(s + inlen, 0, 10);

				*ival = (inarg < INT_MAX) ? (int) inarg : INT_MAX;
			}
			i++;
		}
		s++;
	} while (s);

	return i;
}

/*** Object kind lookup functions ***/

/**
 * Return the object kind with the given `tval` and `sval`, or NULL.
 */
struct object_kind *lookup_kind(int tval, int sval)
{
	int k;

	/* Look for it */
	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];
		if (kind->tval == tval && kind->sval == sval)
			return kind;
	}

	/* Failure */
	msg("No object: %d:%d (%s)", tval, sval, tval_find_name(tval));
	return NULL;
}

struct object_kind *objkind_byid(int kidx) {
	if (kidx < 0 || kidx >= z_info->k_max)
		return NULL;
	return &k_info[kidx];
}


/*** Textual<->numeric conversion ***/

/**
 * Return the a_idx of the artifact with the given name
 */
const struct artifact *lookup_artifact_name(const char *name)
{
	int i;
	int a_idx = -1;

	/* Look for it */
	for (i = 0; i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];

		/* Test for equality */
		if (art->name && streq(name, art->name))
			return art;
		
		/* Test for close matches */
		if (strlen(name) >= 3 && art->name && my_stristr(art->name, name)
			&& a_idx == -1)
			a_idx = i;
	}

	/* Return our best match */
	return a_idx > 0 ? &a_info[a_idx] : NULL;
}

/**
 * \param name ego type name
 * \param tval object tval
 * \param sval object sval
 * \return eidx of the ego item type
 */
struct ego_item *lookup_ego_item(const char *name, int tval, int sval)
{
	struct object_kind *kind = lookup_kind(tval, sval);
	int i;

	/* Look for it */
	if (!kind) return NULL;
	for (i = 0; i < z_info->e_max; i++) {
		struct ego_item *ego = &e_info[i];
		struct poss_item *poss_item = ego->poss_items;

		/* Reject nameless and wrong names */
		if (!ego->name) continue;
		if (!streq(name, ego->name)) continue;

		/* Check tval and sval */
		while (poss_item) {
			if (kind->kidx == poss_item->kidx) {
				return ego;
			}
			poss_item = poss_item->next;
		}
	}

	return NULL;
}

/**
 * Return the numeric sval of the object kind with the given `tval` and
 * name `name`.
 */
int lookup_sval(int tval, const char *name)
{
	int k;
	char *pe;
	unsigned long r = strtoul(name, &pe, 10);

	if (pe != name) {
		return (contains_only_spaces(pe) && r < INT_MAX) ? (int)r : -1;
	}

	/* Look for it */
	for (k = 0; k < z_info->k_max; k++) {
		struct object_kind *kind = &k_info[k];
		char cmp_name[1024];

		if (!kind || !kind->name || kind->tval != tval) continue;

		obj_desc_name_format(cmp_name, sizeof cmp_name, 0, kind->name, 0,
							 false);

		/* Found a match */
		if (!my_stricmp(cmp_name, name)) return kind->sval;
	}

	return -1;
}

void object_short_name(char *buf, size_t max, const char *name)
{
	size_t j, k;
	/* Copy across the name, stripping modifiers & and ~) */
	size_t len = strlen(name);
	for (j = 0, k = 0; j < len && k < max - 1; j++) {
		if (j == 0 && name[0] == '&' && name[1] == ' ')
			j += 2;
		if (name[j] == '~')
			continue;

		buf[k++] = name[j];
	}
	buf[k] = 0;
}

/**
 * Sort comparator for objects using only tval and sval.
 * -1 if o1 should be first
 *  1 if o2 should be first
 *  0 if it doesn't matter
 */
static int compare_types(const struct object *o1, const struct object *o2)
{
	if (o1->tval == o2->tval)
		return CMP(o1->sval, o2->sval);
	else
		return CMP(o1->tval, o2->tval);
}	


/**
 * Sort comparator for objects
 * -1 if o1 should be first
 *  1 if o2 should be first
 *  0 if it doesn't matter
 *
 * The sort order is designed with the "list items" command in mind.
 */
int compare_items(const struct object *o1, const struct object *o2)
{
	/* unknown objects go at the end, order doesn't matter */
	if (is_unknown(o1)) {
		return (is_unknown(o2)) ? 0 : 1;
	} else if (is_unknown(o2)) {
		return -1;
	}

	/* known artifacts will sort first */
	if (object_is_known_artifact(o1) && object_is_known_artifact(o2))
		return compare_types(o1, o2);
	if (object_is_known_artifact(o1)) return -1;
	if (object_is_known_artifact(o2)) return 1;

	/* unknown objects will sort next */
	if (!object_flavor_is_aware(o1) && !object_flavor_is_aware(o2))
		return compare_types(o1, o2);
	if (!object_flavor_is_aware(o1)) return -1;
	if (!object_flavor_is_aware(o2)) return 1;

	/* if only one of them is worthless, the other comes first */
	if (o1->kind->cost == 0 && o2->kind->cost != 0) return 1;
	if (o1->kind->cost != 0 && o2->kind->cost == 0) return -1;

	/* otherwise, just compare tvals and svals */
	/* NOTE: arguably there could be a better order than this */
	return compare_types(o1, o2);
}


/**
 * Convert a depth from a chunk or player to a value appropriate for an
 * object's origin.
 *
 * \param depth is the value to convert.
 *
 * Necessary since savefiles use 16-bit type to record the depth of a player
 * or chunk and uint8_t to record the origin depth.
 */
uint8_t convert_depth_to_origin(int depth)
{
	if (depth < 0) return 0;
	if (depth > 255) return 255;
	return (uint8_t) depth;
}


/**
 * Determine if an object has charges
 */
bool obj_has_charges(const struct object *obj)
{
	if (!tval_can_have_charges(obj)) return false;

	if (obj->pval <= 0) return false;

	return true;
}

/**
 * Determine if an object is zappable
 */
bool obj_can_zap(const struct object *obj)
{
	/* Any rods not charging? */
	if (tval_can_have_timeout(obj) && number_charging(obj) < obj->number)
		return true;

	return false;
}

/**
 * Determine if an object is activatable
 */
bool obj_is_activatable(const struct object *obj)
{
	if (!tval_is_wearable(obj)) return false;
	return object_effect(obj) ? true : false;
}

/**
 * Determine if an object can be activated now
 */
bool obj_can_activate(const struct object *obj)
{
	if (obj_is_activatable(obj)) {
		/* Check the recharge */
		if (!obj->timeout) return true;
	}

	return false;
}

/**
 * Check if an object can be used to refuel other objects.
 */
bool obj_can_refill(const struct object *obj)
{
	const struct object *light = equipped_item_by_slot_name(player, "light");

	/* Need fuel? */
	if (of_has(obj->flags, OF_NO_FUEL)) return false;

	/* A lantern can be refueled from a flask or another lantern */
	if (light && of_has(light->flags, OF_TAKES_FUEL)) {
		if (tval_is_fuel(obj))
			return true;
		else if (tval_is_light(obj) && of_has(obj->flags, OF_TAKES_FUEL) &&
				 obj->timeout > 0) 
			return true;
	}

	return false;
}

bool obj_kind_can_browse(const struct object_kind *kind)
{
	int i;

	for (i = 0; i < player->class->magic.num_books; i++) {
		struct class_book book = player->class->magic.books[i];
		if (kind->tval == book.tval && kind->sval == book.sval)
			return true;
	}

	return false;
}

bool obj_can_browse(const struct object *obj)
{
	return obj_kind_can_browse(obj->kind);
}

bool obj_can_cast_from(const struct object *obj)
{
	return obj_can_browse(obj) &&
		spell_book_count_spells(player, obj, spell_okay_to_cast) > 0;
}

bool obj_can_study(const struct object *obj)
{
	return obj_can_browse(obj) &&
		spell_book_count_spells(player, obj, spell_okay_to_study) > 0;
}


/* Can only take off non-cursed items */
bool obj_can_takeoff(const struct object *obj)
{
	return !obj_has_flag(obj, OF_STICKY);
}

/*
 * Can only throw an item that is not equipped or the equipped weapon if it
 * can be taken off.
 */
bool obj_can_throw(const struct object *obj)
{
	return !object_is_equipped(player->body, obj)
		|| (tval_is_melee_weapon(obj) && obj_can_takeoff(obj));
}

/* Can only put on wieldable items */
bool obj_can_wear(const struct object *obj)
{
	return (wield_slot(obj) >= 0);
}

/* Can only fire an item with the right tval */
bool obj_can_fire(const struct object *obj)
{
	return obj->tval == player->state.ammo_tval;
}

/**
 * Determine if an object is designed for throwing
 */
bool obj_is_throwing(const struct object *obj)
{
	return of_has(obj->flags, OF_THROWING);
}

/**
 * Determine if an object is a known artifact
 */
bool obj_is_known_artifact(const struct object *obj)
{
	if (!obj->artifact) return false;
	if (!obj->known) return false;
	if (!obj->known->artifact) return false;
	return true;
}

/* Can has inscrip pls */
bool obj_has_inscrip(const struct object *obj)
{
	return (obj->note ? true : false);
}

bool obj_has_flag(const struct object *obj, int flag)
{
	struct curse_data *c = obj->curses;

	/* Check the object's own flags */
	if (of_has(obj->flags, flag)) {
		return true;
	}

	/* Check any curse object flags */
	if (c) {
		int i;
		for (i = 1; i < z_info->curse_max; i++) {
			if (c[i].power && of_has(curses[i].obj->flags, flag)) {
				return true;
			}
		}
	}
	return false;
}

bool obj_is_useable(const struct object *obj)
{
	if (tval_is_useable(obj))
		return true;

	if (object_effect(obj))
		return true;

	if (tval_is_ammo(obj))
		return obj->tval == player->state.ammo_tval;

	return false;
}

/*** Generic utility functions ***/

/**
 * Return an object's effect.
 */
struct effect *object_effect(const struct object *obj)
{
	if (obj->activation)
		return obj->activation->effect;
	else if (obj->effect)
		return obj->effect;
	else
		return NULL;
}

/**
 * Does the given object need to be aimed?
 */ 
bool obj_needs_aim(const struct object *obj)
{
	const struct effect *effect = object_effect(obj);

	/* If the effect needs aiming, or if the object type needs
	   aiming, this object needs aiming. */
	return effect_aim(effect) || tval_is_ammo(obj) ||
			tval_is_wand(obj) ||
			(tval_is_rod(obj) && !object_flavor_is_aware(obj));
}

/**
 * Can the object fail if used?
 */
bool obj_can_fail(const struct object *o)
{
	if (tval_can_have_failure(o))
		return true;

	return wield_slot(o) == -1 ? false : true;
}


/**
 * Failure rate for magic devices.
 * This has been rewritten for 4.2.3 following the discussions in the thread
 * https://angband.live/forums/forum/angband/development/9911-please-help-md-negative-value
 * It uses a scaled, shifted version of the sigmoid function x/(1+|x|), namely
 * 380 - 370(x/(5+|x|)), where x is 2 * (device skill - device level) + 1,
 * to give fail rates out of 1000.
 */
int get_use_device_chance(const struct object *obj)
{
	int lev, fail, x;
	int skill = player->state.skills[SKILL_DEVICE];

	/* Extract the item level, which is the difficulty rating */
	lev = obj->artifact ? obj->artifact->level :
		(obj->activation ? obj->activation->level : obj->kind->level);

	/* Calculate x */
	x = 2 * (skill - lev) + 1;

	/* Now calculate the failure rate */
	fail = -370 * x;
	fail /= (5 + ABS(x));
	fail += 380;

	return fail;
}


/**
 * Distribute charges of rods, staves, or wands.
 *
 * \param source is the source item
 * \param dest is the target item, must be of the same type as source
 * \param amt is the number of items that are transfered
 * \param dest_new will, if true, ignore whatever charges or timeout, dest
 * has (i.e. treat it as a new stack).
 */
void distribute_charges(struct object *source, struct object *dest, int amt,
		bool dest_new)
{
	/*
	 * If rods, staves, or wands are dropped, the total maximum
	 * timeout or charges need to be allocated between the two stacks.
	 * If all the items are being dropped, it makes for a neater message
	 * to leave the original stack's pval alone. -LM-
	 */
	if (tval_can_have_charges(source)) {
		int change = source->pval * amt / source->number;

		if (dest_new) {
			dest->pval = change;
		} else {
			dest->pval += change;
		}
		if (amt < source->number) {
			source->pval -= change;
		}
	}

	/*
	 * Rods also need to have their timeouts distributed.
	 *
	 * The dropped stack will accept all time remaining to charge up to
	 * its maximum.
	 */
	if (tval_can_have_timeout(source)) {
		int charge_time = randcalc(source->time, 0, AVERAGE), max_time;

		max_time = charge_time * amt;
		if (dest_new) {
			dest->timeout = (source->timeout > max_time)
				? max_time : source->timeout;
			if (amt < source->number) {
				source->timeout -= dest->timeout;
			}
		} else {
			int change = (source->timeout > max_time)
				? max_time : source->timeout;

			max_time = charge_time * (dest->number + amt);
			if (dest->timeout < max_time) {
				if (change > max_time - dest->timeout) {
					change = max_time - dest->timeout;
				}
				dest->timeout += change;
				if (amt < source->number) {
					source->timeout -= change;
				}
			}
		}
	}
}


/**
 * Number of items (usually rods) charging
 */
int number_charging(const struct object *obj)
{
	int charge_time, num_charging;

	charge_time = randcalc(obj->time, 0, AVERAGE);

	/* Item has no timeout */
	if (charge_time <= 0) return 0;

	/* No items are charging */
	if (obj->timeout <= 0) return 0;

	/* Calculate number charging based on timeout */
	num_charging = (obj->timeout + charge_time - 1) / charge_time;

	/* Number charging cannot exceed stack size */
	if (num_charging > obj->number) num_charging = obj->number;

	return num_charging;
}

/**
 * Allow a stack of charging objects to charge by one unit per charging object
 * Return true if something recharged
 */
bool recharge_timeout(struct object *obj)
{
	int charging_before, charging_after;

	/* Find the number of charging items */
	charging_before = number_charging(obj);

	/* Nothing to charge */	
	if (charging_before == 0)
		return false;

	/* Decrease the timeout */
	obj->timeout -= MIN(charging_before, obj->timeout);

	/* Find the new number of charging items */
	charging_after = number_charging(obj);

	/* Return true if at least 1 item obtained a charge */
	if (charging_after < charging_before)
		return true;
	else
		return false;
}

/**
 * Verify the choice of an item.
 *
 * The item can be negative to mean "item on floor".
 */
bool verify_object(const char *prompt, const struct object *obj,
		const struct player *p)
{
	char o_name[80];

	char out_val[160];

	/* Describe */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL, p);

	/* Prompt */
	strnfmt(out_val, sizeof(out_val), "%s %s? ", prompt, o_name);

	/* Query */
	return (get_check(out_val));
}


typedef enum {
	MSG_TAG_NONE,
	MSG_TAG_NAME,
	MSG_TAG_KIND,
	MSG_TAG_VERB,
	MSG_TAG_VERB_IS
} msg_tag_t;

static msg_tag_t msg_tag_lookup(const char *tag)
{
	if (strncmp(tag, "name", 4) == 0) {
		return MSG_TAG_NAME;
	} else if (strncmp(tag, "kind", 4) == 0) {
		return MSG_TAG_KIND;
	} else if (strncmp(tag, "s", 1) == 0) {
		return MSG_TAG_VERB;
	} else if (strncmp(tag, "is", 2) == 0) {
		return MSG_TAG_VERB_IS;
	} else {
		return MSG_TAG_NONE;
	}
}

/**
 * Print a message from a string, customised to include details about an object
 */
void print_custom_message(const struct object *obj, const char *string,
		int msg_type, const struct player *p)
{
	char buf[1024] = "\0";
	const char *next;
	const char *s;
	const char *tag;
	size_t end = 0;

	/* Not always a string */
	if (!string) return;

	next = strchr(string, '{');
	while (next) {
		/* Copy the text leading up to this { */
		strnfcat(buf, 1024, &end, "%.*s", (int) (next - string),
			string);

		s = next + 1;
		while (*s && isalpha((unsigned char) *s)) s++;

		/* Valid tag */
		if (*s == '}') {
			/* Start the tag after the { */
			tag = next + 1;
			string = s + 1;

			switch(msg_tag_lookup(tag)) {
			case MSG_TAG_NAME:
				if (obj) {
					end += object_desc(buf, 1024, obj,
						ODESC_PREFIX | ODESC_BASE, p);
				} else {
					strnfcat(buf, 1024, &end, "hands");
				}
				break;
			case MSG_TAG_KIND:
				if (obj) {
					object_kind_name(&buf[end], 1024 - end, obj->kind, true);
					end += strlen(&buf[end]);
				} else {
					strnfcat(buf, 1024, &end, "hands");
				}
				break;
			case MSG_TAG_VERB:
				if (obj && obj->number == 1) {
					strnfcat(buf, 1024, &end, "s");
				}
				break;
			case MSG_TAG_VERB_IS:
				if ((!obj) || (obj->number > 1)) {
					strnfcat(buf, 1024, &end, "are");
				} else {
					strnfcat(buf, 1024, &end, "is");
				}
			default:
				break;
			}
		} else
			/* An invalid tag, skip it */
			string = next + 1;

		next = strchr(string, '{');
	}
	strnfcat(buf, 1024, &end, "%s", string);

	msgt(msg_type, "%s", buf);
}

/**
 * Return if the given artifact has been created.
 */
bool is_artifact_created(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].created;
}

/**
 * Return if the given artifact has been seen.
 */
bool is_artifact_seen(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].seen;
}

/**
 * Return if the given artifact has ever been seen.
 */
bool is_artifact_everseen(const struct artifact *art)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	return aup_info[art->aidx].everseen;
}

/**
 * Set whether the given artifact has been created or not.
 */
void mark_artifact_created(const struct artifact *art, bool created)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].created = created;
}

/**
 * Set whether the given artifact has been created or not.
 */
void mark_artifact_seen(const struct artifact *art, bool seen)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].seen = seen;
}

/**
 * Set whether the given artifact has been seen or not.
 */
void mark_artifact_everseen(const struct artifact *art, bool seen)
{
	assert(art->aidx == aup_info[art->aidx].aidx);
	aup_info[art->aidx].everseen = seen;
}

/**
 * ------------------------------------------------------------------------
 * Artifacts made in play (ZangbandTK, DEC-170)
 *
 * Zangband's Scroll of Artifact Creation turns an ordinary weapon or piece of
 * armour into an artifact during play.  An artifact here is an entry in
 * a_info, read from the data files at startup, and an object points at its
 * entry; so a made artifact takes an entry of its own.  The parsers allocate
 * a_info with ART_MADE_MAX entries spare beyond the standard set, and a made
 * artifact takes the next of them: the table never moves, so no object's
 * pointer into it goes stale.  z_info->a_max counts the entries in use,
 * standard and made, which is what every loop over the table already reads;
 * the made ones are kept out of generation by an alloc_prob of 0.
 *
 * Their definitions are saved in the "made artifacts" block (save.c), which
 * loads before anything that names an artifact.
 * ------------------------------------------------------------------------ */

/** How many entries of a_info the data files filled, counting the empty 0. */
static int art_standard_max;

/**
 * Remember where the standard artifacts end.  Called by the parsers once they
 * have filled a_info, so it is right for whichever set -- standard or random
 * -- was parsed last, and any made artifacts went with the old table.
 */
void artifacts_note_standard(void)
{
	art_standard_max = z_info->a_max;
}

/** The number of a_info entries the data files filled, counting the 0. */
int artifact_standard_count(void)
{
	return art_standard_max;
}

/** Whether this artifact was made in play rather than read from a file. */
bool artifact_is_made(const struct artifact *art)
{
	return art && (int) art->aidx >= art_standard_max;
}

/** How many artifacts have been made in this game. */
int artifact_made_count(void)
{
	return z_info->a_max - art_standard_max;
}

/**
 * Take the next spare entry for an artifact being made, cleared and indexed,
 * or NULL if every spare entry is in use.  It is not counted in z_info->a_max
 * until artifact_made_commit(), so nothing iterating the table sees a half-made
 * artifact; artifact_made_release() hands it back unused.
 */
struct artifact *artifact_made_claim(void)
{
	struct artifact *art;
	int aidx = z_info->a_max;

	if (aidx >= art_standard_max + ART_MADE_MAX) return NULL;

	art = &a_info[aidx];
	memset(art, 0, sizeof(*art));
	art->aidx = aidx;
	memset(&aup_info[aidx], 0, sizeof(aup_info[aidx]));
	aup_info[aidx].aidx = aidx;
	return art;
}

/** Count a claimed entry as a real artifact. */
void artifact_made_commit(struct artifact *art)
{
	assert((int) art->aidx == z_info->a_max);
	z_info->a_max++;
}

/** Free what an entry holds and clear it. */
static void artifact_made_clear(struct artifact *art)
{
	uint32_t aidx = art->aidx;

	string_free(art->name);
	string_free(art->text);
	string_free(art->alt_msg);
	mem_free(art->brands);
	mem_free(art->slays);
	mem_free(art->curses);
	memset(art, 0, sizeof(*art));
	memset(&aup_info[aidx], 0, sizeof(aup_info[aidx]));
}

/** Hand back a claimed entry that was never committed. */
void artifact_made_release(struct artifact *art)
{
	assert((int) art->aidx == z_info->a_max);
	artifact_made_clear(art);
}

/**
 * Forget every made artifact.  A savefile is loaded into a game that may have
 * run another character before it, and the made artifacts are that
 * character's; the load puts back its own.
 */
void artifact_made_reset(void)
{
	int i;

	if (!a_info || !z_info) return;
	for (i = art_standard_max; i < z_info->a_max; i++) {
		artifact_made_clear(&a_info[i]);
	}
	if (art_standard_max) z_info->a_max = art_standard_max;
}

/**
 * Whether any artifact already has this name, exactly.  Objects are saved
 * with their artifact's name and loaded by looking it up, so two artifacts
 * sharing one would come back as the same artifact.
 */
bool artifact_name_taken(const char *name)
{
	int i;

	for (i = 0; i < z_info->a_max; i++) {
		if (a_info[i].name && streq(a_info[i].name, name)) return true;
	}
	return false;
}
