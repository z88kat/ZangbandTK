/**
 * \file player-virtue.c
 * \brief The virtues a character is measured against (PLR-18 to PLR-21)
 *
 * Zangband's avatar system, finished. Topi Ylinen wrote it in 1998 as an
 * Ultima IV-style measure of how a character behaves, and it was never
 * completed: by 2.7.5-pre1 there were 168 places that *wrote* a virtue and not
 * one that read a virtue back. The knowledge screen that would have shown them
 * was commented out with the note "Display virtues option is always left out"
 * ([cmd4.c:4214](../archive/zangband/src/cmd4.c#L4214)). It was accounting
 * with the display switched off.
 *
 * What is kept, because it is the part that did real work: the selection. A
 * character is measured against **eight** virtues out of eighteen, chosen at
 * birth from their class, their race and their magic realm, deduplicated, and
 * padded from a weighted table. Two Warriors are asked about different things.
 *
 * What is new: the two consumers, and enough writers that every virtue a
 * character can be given is one their play can move. See DEC-43 for why that
 * is the rule rather than porting all 168 of Zangband's nudges.
 *
 * Copyright (c) 2026 ZangbandTK contributors
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

#include "player-virtue.h"

#include "init.h"
#include "monster.h"
#include "mon-predicate.h"
#include "player-timed.h"

static const char *virtue_names[] = {
	"(none)",
	#define V(a, b) b,
	#include "list-virtues.h"
	#undef V
};

/*
 * The symbols, which are what the data files say. Kept apart from the display
 * names because for one virtue they differ: `ENCHANT` in class.txt is
 * Mysticism on screen, and a data file that said "Mysticism" would not match
 * anything in the source.
 */
static const char *virtue_codes[] = {
	"NONE",
	#define V(a, b) #a,
	#include "list-virtues.h"
	#undef V
};

/**
 * The weighted table the empty slots are filled from.
 *
 * Zangband rolls 1d29 over nine of the eighteen
 * ([avatar.c:68](../archive/zangband/src/avatar.c#L68)); the weights are the
 * widths of its case ranges, reproduced rather than rounded. The nine are the
 * virtues any character might plausibly be measured against -- the other nine
 * are specific enough that they arrive through class, race or realm or not at
 * all, which is why a Necromancer can be asked about Unlife and a Warrior
 * never is.
 */
static const struct {
	int virtue;
	int weight;
} virtue_pad[] = {
	{ V_SACRIFICE, 3 }, { V_COMPASSION, 3 }, { V_VALOUR, 6 },
	{ V_HONOUR, 5 }, { V_JUSTICE, 4 }, { V_TEMPERANCE, 2 },
	{ V_HARMONY, 2 }, { V_PATIENCE, 3 }, { V_DILIGENCE, 1 }
};

const char *virtue_code(int virtue)
{
	if (virtue <= V_NONE || virtue >= V_MAX) return "NONE";

	return virtue_codes[virtue];
}

const char *virtue_name(int virtue)
{
	if (virtue <= V_NONE || virtue >= V_MAX) return "(none)";

	return virtue_names[virtue];
}

bool player_has_virtue(const struct player *p, int virtue)
{
	int i;

	if (!p || virtue <= V_NONE) return false;

	for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
		if (p->vir_types[i] == virtue) return true;
	}

	return false;
}

/**
 * Where a character stands in a virtue, or zero if they are not measured
 * against it at all.
 *
 * Returning zero for a virtue the character does not hold is what lets a
 * consumer name any of the eighteen without first asking whether this
 * character has it: a Warrior who was never asked about Mysticism is neither
 * for it nor against it.
 */
int virtue_value(const struct player *p, int virtue)
{
	int i;

	if (!p || virtue <= V_NONE) return 0;

	for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
		if (p->vir_types[i] == virtue) return p->virtues[i];
	}

	return 0;
}

void virtue_change(struct player *p, int virtue, int amount)
{
	int i;

	if (!p || virtue <= V_NONE || !amount) return;

	for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
		if (p->vir_types[i] != virtue) continue;

		p->virtues[i] = MAX(-VIRTUE_CAP,
							MIN(VIRTUE_CAP, p->virtues[i] + amount));
		return;
	}
}

/** Add a virtue to the character's eight, if there is room and it is new. */
static void virtue_add(struct player *p, int virtue, int *next)
{
	if (virtue <= V_NONE || virtue >= V_MAX) return;
	if (*next >= MAX_PLAYER_VIRTUES) return;
	if (player_has_virtue(p, virtue)) return;

	p->vir_types[(*next)++] = virtue;
}

/** Fill one empty slot from the weighted table. */
static void virtue_pad_slot(struct player *p, int slot)
{
	int total = 0, roll, i;

	for (i = 0; i < (int) N_ELEMENTS(virtue_pad); i++) {
		if (player_has_virtue(p, virtue_pad[i].virtue)) continue;
		total += virtue_pad[i].weight;
	}

	/* Everything in the table is already held; leave the slot empty. */
	if (!total) return;

	roll = randint0(total);
	for (i = 0; i < (int) N_ELEMENTS(virtue_pad); i++) {
		if (player_has_virtue(p, virtue_pad[i].virtue)) continue;
		if (roll < virtue_pad[i].weight) {
			p->vir_types[slot] = virtue_pad[i].virtue;
			return;
		}
		roll -= virtue_pad[i].weight;
	}
}

/**
 * Choose the eight virtues a character will be measured against (PLR-19).
 *
 * Class first, then race, then the realm they cast from, then the weighted
 * table for whatever is left over. Duplicates are dropped as they arise rather
 * than swept up afterwards, which is the one place this departs from
 * Zangband's shape: it deduplicated at the end and left holes, which came to
 * the same thing by a longer road.
 *
 * Zangband's realm rule -- take Vitality, or Compassion if the character
 * already has Vitality -- is not a special case here. `virtues:` is an ordered
 * list at every level and the first entry not already held is the one taken,
 * so `virtues:VITALITY | COMPASSION` on the divine realm says the same thing
 * without a rule of its own.
 */
void virtues_select(struct player *p)
{
	int next = 0, i;

	for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
		p->vir_types[i] = V_NONE;
		p->virtues[i] = 0;
	}

	if (!p->class || !p->race) return;

	for (i = 0; i < MAX_CLASS_VIRTUES; i++) {
		virtue_add(p, p->class->virtues[i], &next);
	}
	for (i = 0; i < MAX_RACE_VIRTUES; i++) {
		virtue_add(p, p->race->virtues[i], &next);
	}

	/*
	 * And one for the realm they cast from, which is the book they start
	 * with. A Warrior has none and gets one more from the table instead.
	 */
	if (p->class->magic.num_books && p->class->magic.books[0].realm) {
		const struct magic_realm *realm = p->class->magic.books[0].realm;
		int before = next;

		for (i = 0; i < MAX_REALM_VIRTUES; i++) {
			virtue_add(p, realm->virtues[i], &next);
			if (next > before) break;
		}
	}

	for (i = 0; i < MAX_PLAYER_VIRTUES; i++) {
		if (p->vir_types[i] == V_NONE) virtue_pad_slot(p, i);
	}
}

/**
 * How a standing reads, on Zangband's thirteen-band ladder
 * ([avatar.c:370](../archive/zangband/src/avatar.c#L370)).
 */
const char *virtue_describe(int value)
{
	if (value < -100) return "the polar opposite of";
	if (value < -80) return "an arch-enemy of";
	if (value < -60) return "a bitter enemy of";
	if (value < -40) return "an enemy of";
	if (value < -20) return "sinning against";
	if (value < 0) return "straying from the path of";
	if (value == 0) return "neutral to";
	if (value < 20) return "somewhat virtuous in";
	if (value < 40) return "virtuous in";
	if (value < 60) return "very virtuous in";
	if (value < 80) return "a champion of";
	if (value < 100) return "a great champion of";

	return "the living embodiment of";
}

/**
 * What killing this creature says about the killer (PLR-20).
 *
 * The densest of Zangband's writers, and the one worth porting whole:
 * [xtra2.c:918](../archive/zangband/src/xtra2.c#L918) reads a dozen things off
 * the dead monster's race and moves eight virtues from them. Every test it
 * makes is one 4.2 has -- a race flag, a level, a depth -- so this is the
 * original's judgement rather than a reconstruction of it.
 *
 * The one substitution: Zangband tested `mon_name_cont(r_ptr, "beggar")` for
 * the creatures it was shameful to kill, matching on the name. This uses the
 * townsfolk base instead, which is the same set without the string matching.
 */
void virtue_note_kill(struct player *p, const struct monster_race *race,
					  int depth)
{
	if (!p || !race) return;

	/* Killing something out of its depth, or well above your own, is brave. */
	if (race->level > depth && randint1(10) <= race->level - depth) {
		virtue_change(p, V_VALOUR, 1);
	}
	if (race->level >= 2 * p->lev) {
		virtue_change(p, V_VALOUR, 1);
	}

	if (rf_has(race->flags, RF_UNIQUE)) {
		/* Removing something singular from the world, either way. */
		if (rf_has(race->flags, RF_EVIL)) {
			virtue_change(p, V_HARMONY, 2);
		}

		/*
		 * Zangband asked whether the dead thing was GOOD here, and 4.2 has no
		 * such flag -- only EVIL. The flag is not recoverable either: Zangband
		 * put GOOD on 230 monsters and they are not a category 4.2 records,
		 * running from Farmer Maggot to Mughash the Kobold Lord.
		 *
		 * `race_is_living()` stands in, and only on the unique branches.
		 * The axis Zangband was measuring is what you destroy -- kill the
		 * living and you lean towards unlife, kill the undead and you lean
		 * away -- and living is the closer of the two predicates 4.2 has to
		 * that. Restricting it to uniques keeps the rate down: GOOD was on a
		 * quarter of the bestiary and living is on nearly all of it, so the
		 * non-unique branch is dropped rather than substituted.
		 */
		if (race_is_living(race)) {
			virtue_change(p, V_UNLIFE, 2);
			virtue_change(p, V_VITALITY, -2);
		}
		if (rf_has(race->flags, RF_UNDEAD)) {
			virtue_change(p, V_VITALITY, 2);
		}
		if (one_in_(3)) virtue_change(p, V_INDIVIDUALISM, -1);
	}

	/* Killing the harmless is not a thing to be proud of. */
	if (race->base && streq(race->base->name, "townsfolk")) {
		virtue_change(p, V_COMPASSION, -1);
	}

	/*
	 * Angels and demons, and the theology of killing either. Zangband asked
	 * whether the glyph was 'A' and whether the thing was evil; 4.2 keeps the
	 * same glyph for angels and has a DEMON flag.
	 */
	if (race->d_char == L'A' && !rf_has(race->flags, RF_EVIL)) {
		if (rf_has(race->flags, RF_UNIQUE)) {
			virtue_change(p, V_FAITH, -2);
		} else if (race->level / 10 + 3 * depth >= randint1(100)) {
			virtue_change(p, V_FAITH, -1);
		}
	} else if (race->d_char == L'A' || rf_has(race->flags, RF_DEMON)) {
		if (rf_has(race->flags, RF_UNIQUE)) {
			virtue_change(p, V_FAITH, 2);
		} else if (race->level / 10 + 3 * depth >= randint1(100)) {
			virtue_change(p, V_FAITH, 1);
		}
	}
}

/**
 * What a timed effect beginning says about the character (PLR-20).
 *
 * Three of Zangband's, all at the moment the effect starts rather than while
 * it runs: haste is impatient and industrious
 * ([effects.c:477](../archive/zangband/src/effects.c#L477)), and invulnerability
 * is four kinds of cowardice at once
 * ([effects.c:993](../archive/zangband/src/effects.c#L993)) -- the largest
 * single virtue penalty anywhere in the original, and pointed at Valour.
 */
void virtue_note_timed(struct player *p, int idx, int old, int new_value)
{
	if (!p || old > 0 || new_value <= 0) return;

	if (idx == TMD_FAST) {
		virtue_change(p, V_PATIENCE, -1);
		virtue_change(p, V_DILIGENCE, 1);
	} else if (idx == TMD_INVULN) {
		virtue_change(p, V_TEMPERANCE, -5);
		virtue_change(p, V_HONOUR, -5);
		virtue_change(p, V_SACRIFICE, -5);
		virtue_change(p, V_VALOUR, -10);
	}
}
