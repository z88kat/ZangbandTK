/**
 * \file  borg-magic.c
 * \brief The basic magic definitions and routines to cast spells
 *
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

#include "borg-magic.h"

#ifdef ALLOW_BORG

#include "../effects.h"
#include "../player-spell.h"
#include "../ui-menu.h"

#include "borg-cave.h"
#include "borg-cave-view.h"
#include "borg-init.h"
#include "borg-io.h"
#include "borg-trait.h"

/*
 * Spell info - individualized for class by spell number 
*/

borg_magic *borg_magics = NULL; 


static borg_spell_rating *borg_spell_ratings;

/*
 * How many entries `borg_spell_ratings` actually has (ZangbandTK, BRG-08).
 *
 * The tables have no terminator and no length, and `borg_init_spell()` indexed
 * them by the spell's *position in the class's spell list* -- so a Mage with
 * 224 spells read `borg_spell_ratings[30..223]` off the end of a 30-entry
 * array. That is the crash, and it is why ten of fourteen classes killed the
 * process rather than reporting anything.
 */
static int borg_spell_rating_count;

/*
 * The rating for a spell of this name, or NULL if it is not in the table.
 *
 * By name rather than by position, which is the whole of BRG-07's argument in
 * miniature: positions moved the moment M9 replaced the spell lists, and names
 * did not. A linear scan over at most thirty entries, run once per spell at
 * borg startup.
 */
static const borg_spell_rating *borg_find_rating(const char *name)
{
    int i;

    if (!name || !borg_spell_ratings) return NULL;

    for (i = 0; i < borg_spell_rating_count; i++) {
        if (borg_spell_ratings[i].name
            && streq(borg_spell_ratings[i].name, name)) {
            return &borg_spell_ratings[i];
        }
    }

    return NULL;
}
// !FIX !TODO for now put this in the code.  It should probably end up in borg.txt or a new borg.cfg
// I also gave low ratings to spells that are new since the borg doesn't know when to use them yet.
static borg_spell_rating borg_spell_ratings_MAGE[] =
{
    { "Magic Missile", 95, MAGIC_MISSILE },
    { "Light Room", 65, LIGHT_ROOM },
    { "Find Traps, Doors & Stairs", 85, FIND_TRAPS_DOORS_STAIRS },
    { "Phase Door", 95, PHASE_DOOR },
    { "Electric Arc", 85, ELECTRIC_ARC },
    { "Detect Monsters", 85, DETECT_MONSTERS },
    { "Fire Ball", 75, FIRE_BALL },
    { "Recharging", 65, RECHARGING },
    { "Identify Rune", 95, IDENTIFY_RUNE },
    { "Treasure Detection", 5, TREASURE_DETECTION }, /* borg never uses this */
    { "Frost Bolt", 75, FROST_BOLT },
    { "Reveal Monsters", 85, REVEAL_MONSTERS },
    { "Acid Spray", 75, ACID_SPRAY },
    { "Disable Traps, Destroy Doors", 95, DISABLE_TRAPS_DESTROY_DOORS },
    { "Teleport Self", 95, TELEPORT_SELF },
    { "Teleport Other", 75, TELEPORT_OTHER },
    { "Resistance", 90, RESISTANCE },
    { "Tap Magical Energy", 5, TAP_MAGICAL_ENERGY }, /* need to figure out when to cast this one */
    { "Mana Channel", 95, MANA_CHANNEL },
    { "Door Creation", 65, DOOR_CREATION },
    { "Mana Bolt", 95, MANA_BOLT },
    { "Teleport Level", 65, TELEPORT_LEVEL },
    { "Detection", 95, DETECTION },
    { "Dimension Door", 95, DIMENSION_DOOR },
    { "Thrust Away", 55, THRUST_AWAY },
    { "Shock Wave", 85, SHOCK_WAVE },
    { "Explosion", 85, EXPLOSION },
    { "Banishment", 75, BANISHMENT },
    { "Mass Banishment", 65, MASS_BANISHMENT },
    { "Mana Storm", 75, MANA_STORM }
};
static borg_spell_rating borg_spell_ratings_DRUID[] =
{
    { "Detect Life", 95,  DETECT_LIFE },
    { "Fox Form", 5, FOX_FORM }, // !FIX !TODO need to know when to cast any of the shapechanges
    { "Remove Hunger", 85, REMOVE_HUNGER },
    { "Stinking Cloud", 95, STINKING_CLOUD },
    { "Confuse Monster", 55, CONFUSE_MONSTER },
    { "Slow Monster", 65, SLOW_MONSTER },
    { "Cure Poison", 55, CURE_POISON },
    { "Resist Poison", 60, RESIST_POISON },
    { "Turn Stone to Mud", 80, TURN_STONE_TO_MUD },
    { "Sense Surroundings", 80, SENSE_SURROUNDINGS },
    { "Lightning Strike", 85, LIGHTNING_STRIKE },
    { "Earth Rising", 70, EARTH_RISING },
    { "Trance", 55, TRANCE },
    { "Mass Sleep", 80, MASS_SLEEP },
    { "Become Pukel-man", 5, BECOME_PUKEL_MAN }, // !FIX !TODO shapechange
    { "Eagle's Flight", 5, EAGLES_FLIGHT }, // !FIX !TODO shapechange
    { "Bear Form", 5, BEAR_FORM }, // !FIX !TODO shapechange
    { "Tremor", 80, TREMOR },
    { "Haste Self", 90, HASTE_SELF },
    { "Revitalize", 95, REVITALIZE },
    { "Rapid Regeneration", 55, RAPID_REGENERATION },
    { "Herbal Curing", 90, HERBAL_CURING },
    { "Meteor Swarm", 90, METEOR_SWARM },
    { "Rift", 90, RIFT },
    { "Ice Storm", 85, ICE_STORM },
    { "Volcanic Eruption", 60, VOLCANIC_ERUPTION },
    { "River of Lightning", 90, RIVER_OF_LIGHTNING }
};

static borg_spell_rating borg_spell_ratings_PRIEST[] =
{
    { "Call Light", 65, CALL_LIGHT },
    { "Detect Evil", 85, DETECT_EVIL },
    { "Minor Healing", 65, MINOR_HEALING },
    { "Bless", 85, BLESS },
    { "Sense Invisible", 75, SENSE_INVISIBLE },
    { "Heroism", 75, HEROISM },
    { "Orb of Draining", 95, ORB_OF_DRAINING },
    { "Spear of Light", 75, SPEAR_OF_LIGHT },
    { "Dispel Undead", 65, DISPEL_UNDEAD },
    { "Dispel Evil", 65, DISPEL_EVIL },
    { "Protection from Evil", 85, PROTECTION_FROM_EVIL },
    { "Remove Curse", 85, REMOVE_CURSE },
    { "Portal", 85, PORTAL },
    { "Remembrance", 75, REMEMBRANCE },
    { "Word of Recall", 95, WORD_OF_RECALL },
    { "Healing", 95, HEALING },
    { "Restoration", 75, RESTORATION },
    { "Clairvoyance", 85, CLAIRVOYANCE },
    { "Enchant Weapon", 75, ENCHANT_WEAPON },
    { "Enchant Armour", 75, ENCHANT_ARMOUR },
    { "Smite Evil", 75, SMITE_EVIL },
    { "Glyph of Warding", 95, GLYPH_OF_WARDING },
    { "Demon Bane", 85, DEMON_BANE },
    { "Banish Evil", 85, BANISH_EVIL },
    { "Word of Destruction", 75, WORD_OF_DESTRUCTION },
    { "Holy Word", 85, HOLY_WORD },
    { "Spear of Orom\xC3\xab", 85, SPEAR_OF_OROME }, /* "Spear of Orom(e + diaresis)" */
    { "Light of Varda", 85, LIGHT_OF_VARDA } /* "Light of Varda"*/
};
static borg_spell_rating borg_spell_ratings_NECROMANCER[] =
{
    { "Nether Bolt", 95, NETHER_BOLT },
    { "Sense Invisible", 85, SENSE_INVISIBLE },
    { "Create Darkness", 5, CREATE_DARKNESS }, 
    { "Bat Form", 5, BAT_FORM }, // !FIX !TODO shapechange
    { "Read Minds", 85, READ_MINDS },
    { "Tap Unlife", 85, TAP_UNLIFE },
    { "Crush", 95, CRUSH },
    { "Sleep Evil", 85, SLEEP_EVIL },
    { "Shadow Shift", 95, SHADOW_SHIFT },
    { "Disenchant", 25, DISENCHANT },
    { "Frighten", 85, FRIGHTEN },
    { "Vampire Strike", 75, VAMPIRE_STRIKE },
    { "Dispel Life", 65, DISPEL_LIFE },
    { "Dark Spear", 65, DARK_SPEAR },
    { "Warg Form", 5, WARG_FORM }, // !FIX !TODO shapechange
    { "Banish Spirits", 65, BANISH_SPIRITS },
    { "Annihilate", 95, ANNIHILATE },
    { "Grond's Blow", 85, GRONDS_BLOW },
    { "Unleash Chaos", 85, UNLEASH_CHAOS },
    { "Fume of Mordor", 75, FUME_OF_MORDOR },
    { "Storm of Darkness", 65, STORM_OF_DARKNESS },
    { "Power Sacrifice", 5, POWER_SACRIFICE },  /* not sure if this is borg happy. */
    { "Zone of Unmagic", 5, ZONE_OF_UNMAGIC },  // !FIX !TODO defense?  not sure how to code. 
    { "Vampire Form", 5, VAMPIRE_FORM }, // !FIX !TODO shapechange
    { "Curse", 65, CURSE },
    { "Command", 5, COMMAND } // !FIX !TODO defense?  not sure how to code. 
};
static borg_spell_rating borg_spell_ratings_PALADIN[] =
{
    { "Bless", 95, BLESS },
    { "Detect Evil", 85, DETECT_EVIL },
    { "Call Light", 85, CALL_LIGHT },
    { "Minor Healing", 95, MINOR_HEALING },
    { "Sense Invisible", 65, SENSE_INVISIBLE },
    { "Heroism", 85, HEROISM },
    { "Protection from Evil", 85, PROTECTION_FROM_EVIL },
    { "Remove Curse", 65, REMOVE_CURSE },
    { "Word of Recall", 95, WORD_OF_RECALL },
    { "Healing", 95, HEALING },
    { "Clairvoyance", 85, CLAIRVOYANCE },
    { "Smite Evil", 55, SMITE_EVIL },
    { "Demon Bane", 55, DEMON_BANE },
    { "Enchant Weapon", 75, ENCHANT_WEAPON },
    { "Enchant Armour", 85, ENCHANT_ARMOUR },
    { "Single Combat", 95, SINGLE_COMBAT } // !FIX !TODO defense?  not sure how to code.
};
static borg_spell_rating borg_spell_ratings_ROGUE[] =
{
    { "Detect Monsters", 85, DETECT_MONSTERS },
    { "Phase Door", 95, PHASE_DOOR },
    { "Object Detection", 55, OBJECT_DETECTION },
    { "Detect Stairs", 55, DETECT_STAIRS },
    { "Recharging", 85, RECHARGING },
    { "Reveal Monsters", 85, REVEAL_MONSTERS },
    { "Teleport Self", 95, TELEPORT_SELF },
    { "Hit and Run", 15, HIT_AND_RUN }, // !FIX !TODO not sure how to code this
    { "Teleport Other", 85, TELEPORT_OTHER },
    { "Teleport Level", 75, TELEPORT_LEVEL }
};
static borg_spell_rating borg_spell_ratings_RANGER[] =
{
    { "Remove Hunger", 95, REMOVE_HUNGER },
    { "Detect Life", 85, DETECT_LIFE },
    { "Herbal Curing", 95, HERBAL_CURING },
    { "Resist Poison", 85, RESIST_POISON },
    { "Turn Stone to Mud", 85, TURN_STONE_TO_MUD },
    { "Sense Surroundings", 75, SENSE_SURROUNDINGS },
    { "Cover Tracks", 25, COVER_TRACKS }, // !FIX !TODO prep?
    { "Create Arrows", 85, CREATE_ARROWS }, // !FIX !TODO 
    { "Haste Self", 95, HASTE_SELF },
    { "Decoy", 5, DECOY }, // !FIX !TODO not sure what to do with this
    { "Brand Ammunition", 95, BRAND_AMMUNITION }
};
static borg_spell_rating borg_spell_ratings_BLACKGUARD[] =
{
    { "Seek Battle", 55, SEEK_BATTLE },
    { "Berserk Strength", 95, BERSERK_STRENGTH },
    { "Whirlwind Attack", 85, WHIRLWIND_ATTACK },
    { "Shatter Stone", 95, SHATTER_STONE },
    { "Leap into Battle", 65, LEAP_INTO_BATTLE },
    { "Grim Purpose", 65, GRIM_PURPOSE },
    { "Maim Foe", 75, MAIM_FOE },
    { "Howl of the Damned", 55, HOWL_OF_THE_DAMNED },
    { "Relentless Taunting", 5, RELENTLESS_TAUNTING }, /* seems to dangerous for borg right now */
    { "Venom", 55, VENOM },
    { "Werewolf Form", 5, WEREWOLF_FORM }, // !FIX !TODO shapechange
    { "Bloodlust", 5, BLOODLUST }, /* seems to dangerous for borg right now */
    { "Unholy Reprieve", 95, UNHOLY_REPRIEVE },
    { "Forceful Blow", 5, FORCEFUL_BLOW }, // !FIX !TODO need to code this 
    { "Quake", 95, QUAKE }
};

/*
 * get the stat used for casting spells
 *
 * Assumes the first spell determines the realm thus stat for all spells
 */
int borg_spell_stat(void)
{
    if (borg_can_cast()) {
        struct class_spell *spell = &(player->class->magic.books[0].spells[0]);
        if (spell != NULL) {
            return spell->realm->stat;
        }
    }

    return -1;
}

/*
 * Does this player cast spells
 */
bool borg_can_cast(void)
{
    return player->class->magic.total_spells != 0;
}

/*
 * Does this player mostly cast spells
 * HACK: Rather than hard code classes, assume any class with
 * more than three books is primarily casting
 * !FIX !TODO consider adding is_primary_caster to class struct
 */
bool borg_primarily_caster(void)
{
    return player->class->magic.num_books > 3;
}

/*
 * get the level at which Heroism (spell) grants Heroism (effect)
 */
int borg_heroism_level(void)
{
    if (borg.trait[BI_CLASS] == CLASS_PRIEST)
        return 20;
    if (borg.trait[BI_CLASS] == CLASS_PALADIN)
        return 15;
    return 99;
}

/*
 * find the index in the books array given the books sval
 */
int borg_get_book_num(int sval)
{
    if (!borg_can_cast())
        return -1;

    for (int book_num = 0; book_num < player->class->magic.num_books;
         book_num++) {
        if (player->class->magic.books[book_num].sval == sval)
            return book_num;
    }
    return -1;
}

/*
 * is this a dungeon book (not a basic book)
 */
bool borg_is_dungeon_book(int tval, int sval)
{
    switch (tval) {
    case TV_PRAYER_BOOK:
    case TV_MAGIC_BOOK:
    case TV_NATURE_BOOK:
    case TV_SHADOW_BOOK:
    case TV_OTHER_BOOK:
        break;
    default:
        return false;
    }

    /* keep track of if this is a book from the dungeon */
    for (int i = 0; i < player->class->magic.num_books; i++) {
        struct class_book book = player->class->magic.books[i];
        if (tval == book.tval && sval == book.sval && book.dungeon)
            return true;
    }

    return false;
}

/*
 * Find the magic structure given a book/entry
 */
borg_magic *borg_get_spell_entry(int book, int entry)
{
    int entry_in_book = 0;

    for (int spell_num = 0; spell_num < player->class->magic.total_spells;
         spell_num++) {
        if (borg_magics[spell_num].book == book) {
            if (entry_in_book == entry)
                return &borg_magics[spell_num];
            entry_in_book++;
        }
    }
    return NULL;
}

/*
 * Find the spell index for a given spell
 */
static int borg_get_spell_number(const enum borg_spells spell)
{
    /* The borg must be able to "cast" spells */
    if (borg_magics == NULL)
        return -1;

    int total_spells = player->class->magic.total_spells;
    for (int spell_num = 0; spell_num < total_spells; spell_num++) {
        if (borg_magics[spell_num].spell_enum == spell)
            return spell_num;
    }

    return -1;
}

/*
 * Find the power (cost in sp) value for a given spell
 */
int borg_get_spell_power(const enum borg_spells spell)
{
    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return -1;

    borg_magic *as = &borg_magics[spell_num];

    return as->power;
}

/*
 * Determine if borg can cast a given spell (when fully rested)
 */
bool borg_spell_legal(const enum borg_spells spell)
{
    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return false;

    borg_magic *as = &borg_magics[spell_num];

    /* The book must be possessed */
    if (borg.book_idx[as->book] < 0)
        return false;

    /* The spell must be "known" */
    if (borg_magics[spell_num].status < BORG_MAGIC_TEST)
        return false;

    /* The spell must be affordable (when rested) */
    if (borg_magics[spell_num].power > borg.trait[BI_MAXSP])
        return false;

    /* Success */
    return true;
}

/*
 * check a spell for a given effect
 */
static bool borg_spell_has_effect(int spell_num, uint16_t effect)
{
    const struct class_spell *cspell = spell_by_index(player, spell_num);
    struct effect            *eff    = cspell->effect;
    while (eff != NULL) {
        if (eff->index == effect)
            return true;
        eff = eff->next;
    }
    return false;
}

/*
 * Determine if borg can cast a given spell (right now)
 */
bool borg_spell_okay(const enum borg_spells spell)
{
    int reserve_mana = 0;

    int spell_num    = borg_get_spell_number(spell);
    if (spell_num < 0)
        return false;

    borg_magic *as = &borg_magics[spell_num];

    /* Dark */
    if (no_light(player))
        return false;

    /* Define reserve_mana for each class */
    switch (borg.trait[BI_CLASS]) {
    case CLASS_MAGE:
        reserve_mana = 6;
        break;
    case CLASS_RANGER:
        reserve_mana = 22;
        break;
    case CLASS_ROGUE:
        reserve_mana = 20;
        break;
    case CLASS_NECROMANCER:
        reserve_mana = 10;
        break;
    case CLASS_PRIEST:
        reserve_mana = 8;
        break;
    case CLASS_PALADIN:
        reserve_mana = 20;
        break;
    case CLASS_BLACKGUARD:
        reserve_mana = 0;
        break;
    }

    /* Low level spell casters should not worry about this */
    if (borg.trait[BI_CLEVEL] < 35)
        reserve_mana = 0;

    /* Require ability (when rested) */
    if (!borg_spell_legal(spell))
        return false;

    /* Blind/confused/amnesia */
    if (borg.trait[BI_ISBLIND] || borg.trait[BI_ISCONFUSED])
        return false;

    /* The spell must be affordable (now) */
    if (as->power > borg.trait[BI_CURSP])
        return false;

    /* Do not cut into reserve mana (for final teleport) */
    if (borg.trait[BI_CURSP] - as->power < reserve_mana) {
        /* nourishing spells okay */
        if (borg_spell_has_effect(spell_num, EF_NOURISH))
            return true;

        /* okay to run away */
        if (borg_spell_has_effect(spell_num, EF_TELEPORT))
            return true;

        /* Magic Missile OK */
        if (MAGIC_MISSILE == spell && borg.trait[BI_CDEPTH] <= 35)
            return true;

        /* others are rejected */
        return false;
    }

    /* Success */
    return true;
}

/*
 * fail rate on a spell
 */
int borg_spell_fail_rate(const enum borg_spells spell)
{
    int chance, minfail;

    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return 100;

    borg_magic *as = &borg_magics[spell_num];

    /* Access the spell  */
    chance = as->sfail;

    /* Reduce failure rate by "effective" level adjustment */
    chance -= 3 * (borg.trait[BI_CLEVEL] - as->level);

    /* Reduce failure rate by stat adjustment */
    chance -= borg.trait[BI_FAIL1];

    /* Fear makes the failrate higher */
    if (borg.trait[BI_ISAFRAID])
        chance += 20;

    /* Extract the minimum failure rate */
    minfail = borg.trait[BI_FAIL2];

    /* Non mage characters never get too good */
    if (!player_has(player, PF_ZERO_FAIL)) {
        if (minfail < 5)
            minfail = 5;
    }

    /* Necromancers are punished by being on lit squares */
    /* necromancers like the dark */
    if (borg.trait[BI_CLASS] == CLASS_NECROMANCER &&
        borg_grids[borg.c.y][borg.c.x].info & BORG_LIGHT) {
        chance += 25;

    }

    /* Minimum failure rate and max */
    if (chance < minfail)
        chance = minfail;
    if (chance > 50)
        chance = 50;

    /* Stunning makes spells harder */
    if (borg.trait[BI_ISHEAVYSTUN])
        chance += 25;
    if (borg.trait[BI_ISSTUN])
        chance += 15;

    /* Amnesia makes it harder */
    if (borg.trait[BI_ISFORGET])
        chance *= 2;

    /* Always a 5 percent chance of working */
    if (chance > 95)
        chance = 95;

    /* Return the chance */
    return (chance);
}

/*
 * same as borg_spell_okay with a fail % check
 */
bool borg_spell_okay_fail(const enum borg_spells spell, int allow_fail)
{
    if (borg_spell_fail_rate(spell) > allow_fail)
        return false;
    return borg_spell_okay(spell);
}

/*
 * Same as borg_spell with a fail % check
 */
bool borg_spell_fail(const enum borg_spells spell, int allow_fail)
{
    if (borg_spell_fail_rate(spell) > allow_fail)
        return false;
    return borg_spell(spell);
}

/*
 * Same as borg_spell_legal with a fail % check
 */
bool borg_spell_legal_fail(const enum borg_spells spell, int allow_fail)
{
    if (borg_spell_fail_rate(spell) > allow_fail)
        return false;
    return borg_spell_legal(spell);
}

/*
 * Attempt to cast a spell
 */
/*
 * The best spell the character knows that does this (ZangbandTK, BRG-07).
 *
 * The borg casts through ninety-five hardcoded `borg_spell(ENUM)` calls, one
 * per spell it was taught about, and `enum borg_spells` is Angband's spell
 * list. M9 replaced the spell lists with seven realms of thirty-two, so
 * **184 of this game's 224 realm spells have no enum and cannot be cast at
 * all** -- Nature has exactly one castable spell. The measured consequence is
 * blunt: a Mage's first attack spell here is `Zap`, the borg only knows how to
 * ask for `Magic Missile`, so it never casts an attack spell, melees with four
 * hit points, and dies at character level one in every seed tried.
 *
 * Rating by enum cannot fix that, which is why the ratings pass moved nothing.
 * Asking for a spell by *what it does* can, and it is also what finally makes
 * the ratings load-bearing: they are the tie-break between two spells that do
 * the same thing.
 *
 * Returns an index into `borg_magics`, or -1. Index rather than enum because
 * the unrated spells all share `BORG_SPELL_UNKNOWN` and an enum lookup could
 * not tell them apart.
 */
int borg_best_spell_with_effect(int effect_idx)
{
    int i, best = -1, best_rating = -1;

    if (!borg_magics) return -1;

    for (i = 0; i < player->class->magic.total_spells; i++) {
        borg_magic *as = &borg_magics[i];

        if (as->effect_index != (uint16_t) effect_idx) continue;

        /* Has to be one it can actually cast right now */
        if (as->status < BORG_MAGIC_TEST) continue;
        if (borg.book_idx[as->book] < 0) continue;
        if (as->power > borg.trait[BI_CURSP]) continue;

        if (as->rating > best_rating) {
            best_rating = as->rating;
            best        = i;
        }
    }

    return best;
}

/*
 * Cast the spell at this index (ZangbandTK, BRG-07).
 *
 * `borg_spell()` looks a spell up by enum and then casts it by book and
 * offset; the cast itself was always index-based, so this is the second half
 * of that function with the lookup removed. It is the only way to cast a spell
 * the borg has no enum for, which is 82 per cent of this game's realm spells.
 */
bool borg_spell_by_index(int spell_num)
{
    int i;
    borg_magic *as;

    if (!borg_magics || spell_num < 0
        || spell_num >= player->class->magic.total_spells) {
        return false;
    }

    as = &borg_magics[spell_num];

    if (no_light(player)) return false;
    if (as->status < BORG_MAGIC_TEST) return false;
    if (as->power > borg.trait[BI_CURSP]) return false;

    i = borg.book_idx[as->book];
    if (i < 0) return false;

    borg_note(format("# Casting %s by effect (%d,%d).", as->name, i,
        as->book_offset));

    borg_keypress('m');
    borg_keypress(all_letters_nohjkl[i]);
    borg_keypress(all_letters_nohjkl[as->book_offset]);

    as->times++;

    return true;
}

bool borg_spell(const enum borg_spells spell)
{
    int i;

    int spell_num = borg_get_spell_number(spell);
    if (spell_num < 0)
        return false;

    borg_magic *as = &borg_magics[spell_num];

    /* Require ability (right now) */
    if (!borg_spell_okay(spell))
        return false;

    /* Look for the book */
    i = borg.book_idx[as->book];

    /* Paranoia */
    if (i < 0)
        return false;

    /* Debugging Info */
    borg_note(format("# Casting %s (%d,%d).", as->name, i, as->book_offset));

    /* Cast a spell */
    borg_keypress('m');
    borg_keypress(all_letters_nohjkl[i]);
    borg_keypress(all_letters_nohjkl[as->book_offset]);

    /* increment the spell counter */
    as->times++;

    /* Success */
    return true;
}

/*
 * Cheat the "spell" info for a single book
 */
static void borg_cheat_spell(int book_num)
{
    struct class_book *book = &player->class->magic.books[book_num];
    for (int spell_num = 0; spell_num < book->num_spells; spell_num++) {
        struct class_spell *cspell = &book->spells[spell_num];
        borg_magic *as = &borg_magics[cspell->sidx];

        /* Note "forgotten" spells */
        if (player->spell_flags[cspell->sidx] & PY_SPELL_FORGOTTEN) {
            /* Forgotten */
            as->status = BORG_MAGIC_LOST;
        }

        /* Note "difficult" spells */
        else if (borg.trait[BI_CLEVEL] < as->level) {
            /* Unknown */
            as->status = BORG_MAGIC_HIGH;
        }

        /* Note "Unknown" spells */
        else if (!(player->spell_flags[cspell->sidx] & PY_SPELL_LEARNED)) {
            /* UnKnown */
            as->status = BORG_MAGIC_OKAY;
        }

        /* Note "untried" spells */
        else if (!(player->spell_flags[cspell->sidx] & PY_SPELL_WORKED)) {
            /* Untried */
            as->status = BORG_MAGIC_TEST;
        }

        /* Note "known" spells */
        else {
            /* Known */
            as->status = BORG_MAGIC_KNOW;
        }
    }
}

/*
 * Cheat the "spell" info
 */
void borg_cheat_spells(void)
{
    int i;

    /* Assume no books */
    for (i = 0; i < BORG_MAX_BOOKS; i++)
        borg.book_idx[i] = -1;

    /* Scan the pack */
    for (i = 0; i < z_info->pack_size; i++) {
        int        book_num;
        borg_item *item = &borg_items[i];

        /*
         * ZangbandTK (BRG-10): bounded by the array as well as by the class.
         * A Mage has 28 books and this array had nine slots.
         */
        for (book_num = 0; book_num < player->class->magic.num_books
                && book_num < BORG_MAX_BOOKS;
            book_num++) {
            struct class_book book = player->class->magic.books[book_num];

            /*
             * ZangbandTK (BRG-11): an empty pack slot is not a spellbook.
             *
             * A `borg_item` for an empty slot is zeroed, so its tval and sval
             * are both 0 -- and it matched any book entry that also read zero,
             * recording that book as being in inventory slot 0. The borg then
             * believed it held a book it did not own, chose a spell from it,
             * and sent `G a d` for ever while the game answered "You cannot
             * learn any new spells from the books you have".
             *
             * That is the study loop recorded as BRG-11 for the Necromancer
             * and Blackguard, and it is why no character in any run had ever
             * successfully studied a single spell -- the exercise report read
             * `learned=0 cast=0 of 224` for that reason and no other.
             */
            if (!item->iqty) continue;

            if (item->tval == book.tval && item->sval == book.sval) {
                /* Note book locations */
                borg.book_idx[book_num] = i;
                break;
            }
        }
    }

    /* only browse spells if casting is possible */
    if (!borg_can_cast())
        return;

    /* XXX XXX XXX Dark */

    /*
     * ZangbandTK (BRG-10): every book, not the first eight.
     *
     * Angband's widest caster has eight books so the literal was harmless
     * there. Here it meant three quarters of a Mage's spells were never
     * browsed and so never learnable.
     */
    for (int book_idx = 0; book_idx < BORG_MAX_BOOKS; book_idx++)         {
        /* Look for the book */
        i = borg.book_idx[book_idx];

        /* Cheat the "spell" screens (all of them) */
        if (i >= 0)
            /* Cheat that page */
            borg_cheat_spell(book_idx);
    }

    return;
}


/*
 * Get the offset in the book this spell is so you can cast it (book) (offset)
 */
static int borg_get_book_offset(int index)
{
    int                       book = 0, count = 0;
    const struct class_magic *magic = &player->class->magic;

    /* Check index validity */
    if (index < 0 || index >= magic->total_spells)
        return -1;

    /* Find the book, count the spells in previous books */
    while (count + magic->books[book].num_spells - 1 < index)
        count += magic->books[book++].num_spells;

    /* Find the spell */
    return index - count;
}

/*
 * initialize the spell data
 */
/*
 * Describe one of the class's spells to the borg (ZangbandTK, BRG-08, BRG-09).
 *
 * Upstream matched the class's spell list against a hand-written C table
 * *positionally, comparing names*, and treated a mismatch as a startup
 * failure. M9 replaced every spell list, so all eight tables mismatch on
 * their first spell -- a Mage's is now `Zap` where the table expects `Magic
 * Missile` -- and the loop then ran on past the end of the table. Measured
 * with `scripts/borg-smoke`: twelve of fourteen classes fail and ten of them
 * kill the process.
 *
 * So an unrated spell is now **merely unloved** rather than fatal. It is
 * described from the game's own data, given a middling rating and
 * `BORG_SPELL_UNKNOWN`, and the borg simply has no special tactic for it.
 * That is the difference between a borg that cannot start as a Priest and one
 * that plays a Priest without knowing that *Orb of Draining* is its best
 * spell.
 *
 * The fallback rating is flat on purpose, and BRG-08 asks for it to be derived
 * from the spell's effect. It is not derived here because that derivation is a
 * judgement about what the borg should prefer, and BRG-07's rubric -- which
 * says what a rating of 50 means as against 95 -- is not written yet. A flat
 * middling value is honest about knowing nothing; a made-up derivation would
 * look like knowledge.
 */
/*
 * What an unrated spell is worth, from what it does (ZangbandTK, BRG-08).
 *
 * BRG-08 asks for a fallback derived from the spell's effect, and this is it.
 * It was a flat 50 until the rating rubric existed, because a made-up
 * derivation would have looked like knowledge; the rubric is written now, so
 * this implements it.
 *
 * **Rule zero of the rubric is survival first**, and it is not a matter of
 * taste: the borg's measured failure is dying at character level three to six,
 * three runs in six, not playing slowly. A borg that reaches depth 30 alive is
 * verification of M8 to M10; one that fights beautifully and dies at 12 is
 * not. So escape, healing and curing sit above attack, and attack above
 * information.
 *
 * The bands are the rubric's:
 *
 *   90-95  saves the character's life or the run
 *   75-89  its best answer to a fight it is in
 *   60-74  information it will act on
 *   51-59  useful and situational (below the borg's own "bored" threshold)
 *   1-49   judged and not wanted
 *
 * 50 is deliberately never returned. The rubric reserves it for *not yet
 * judged*, and the point of this function is that every spell has now been
 * judged -- by category rather than one at a time, which is the only way 175
 * unrated spells stay consistent with each other.
 *
 * Rating by category also means a spell added tomorrow is rated the day it is
 * added, rather than waiting for somebody to remember it. Where a category is
 * wrong for a particular spell, that is what a per-spell override is for; none
 * is needed yet, and adding the mechanism before a case demands it would be
 * building a pipeline to tune numbers nobody has looked at.
 */
static uint8_t borg_rating_from_effect(int effect_idx)
{
    switch (effect_idx) {
    /*** 90-95: stays alive ***/
    case EF_TELEPORT:            /* the escape, and the borg's best trick */
    case EF_TELEPORT_LEVEL:
        return 95;
    case EF_HEAL_HP:
    case EF_CURE:
        return 92;
    case EF_RESTORE_MANA:        /* a caster with no mana cannot escape either */
        return 90;

    /*** 75-89: wins the fight it is in ***/
    case EF_BALL:
    case EF_SPHERE:
        return 85;
    case EF_BOLT:
    case EF_BOLT_OR_BEAM:
    case EF_BEAM:
    case EF_BOLT_AWARE:
    case EF_BOLT_STATUS:
    case EF_LINE:
        return 82;
    case EF_PROJECT_LOS:
    case EF_PROJECT_LOS_AWARE:   /* sleep or scare a room: survival, really */
        return 88;

    /*** 60-74: information it acts on ***/
    case EF_DETECT_VISIBLE_MONSTERS:
    case EF_DETECT_INVISIBLE_MONSTERS:
    case EF_DETECT_EVIL:
        return 72;
    case EF_DETECT_TRAPS:
    case EF_DETECT_DOORS:
    case EF_DETECT_STAIRS:
        return 68;
    case EF_MAP_AREA:
        return 65;
    case EF_LIGHT_AREA:
        return 62;

    /*** 51-59: worth doing when there is nothing better ***/
    case EF_TIMED_INC:           /* blessings, resistances, speed */
    case EF_TIMED_INC_NO_RES:
        return 58;
    case EF_BRAND_WEAPON:
        return 56;
    case EF_RECHARGE:
    case EF_RESTORE_STAT:
    case EF_RESTORE_EXP:
        return 54;
    case EF_NOURISH:             /* food matters, and it is cheaper to carry */
        return 52;
    case EF_RECALL:
    case EF_IDENTIFY:
        return 51;

    /*** 1-49: judged, and not wanted by an automaton ***/
    case EF_SUMMON_PET:
        /*
         * Pets are M10's and the borg has no idea what to do with them
         * (BRG-15 stops it attacking its own; BRG-16 would have it use them).
         * Rated low rather than zero because a pet still fights for it, and
         * the mana upkeep it cannot reason about is the reason it is not
         * higher: DEC-61 charges the whole stable once the free allowance is
         * passed, which would leave a caster unable to cast.
         */
        return 40;
    case EF_TELEPORT_TO:         /* pulls a monster to you: rarely what it wants */
        return 30;
    case EF_RANDOM:
        /* A random effect is one the borg cannot plan around. */
        return 20;

    default:
        /*
         * Anything not named above. 55 rather than 50: the rubric reserves 50
         * for unjudged, and this *is* a judgement -- "probably useful, nothing
         * special" -- which sits just above the borg's bored threshold.
         */
        return 55;
    }
}

static void borg_init_spell(borg_magic *spells, int spell_num)
{
    borg_magic               *spell  = &spells[spell_num];
    const struct class_spell *cspell = spell_by_index(player, spell_num);
    const borg_spell_rating  *rating;

    /*
     * A spell the game will not describe. Not expected, and not a reason to
     * refuse to play: leave the slot as `mem_zalloc` left it, but with an
     * enum that does not read as Magic Missile.
     */
    if (!cspell) {
        spell->name       = "(unknown)";
        spell->spell_enum = BORG_SPELL_UNKNOWN;
        spell->status     = BORG_MAGIC_ICKY;
        return;
    }

    rating = borg_find_rating(cspell->name);

    spell->rating     = rating ? rating->rating
        : borg_rating_from_effect(cspell->effect ? (int) cspell->effect->index
                                                 : -1);
    spell->spell_enum = rating ? rating->spell_enum : BORG_SPELL_UNKNOWN;

    /*
     * The name comes from the game rather than from the table, so a note
     * about a spell says what the character is actually holding.
     */
    spell->name        = cspell->name;
    spell->level       = cspell->slevel;
    spell->book_offset = borg_get_book_offset(cspell->sidx);

    /*
     * `player/realm` has a test called `a-spell-without-an-effect-says-so`,
     * so a spell with no effect chain is a thing this game's data contains.
     * Upstream dereferenced `cspell->effect` unconditionally.
     */
    spell->effect_index = cspell->effect ? cspell->effect->index : 0;

    spell->power  = cspell->smana;
    spell->sfail  = cspell->sfail;
    spell->status = spell_okay_to_cast(player, spell_num);
    spell->times  = 0;
    spell->book   = cspell->bidx;
}

/*
 * Prepare a book
 */
void borg_prepare_book_info(void)
{
    switch (player->class->cidx) {
    case CLASS_MAGE:
        borg_spell_ratings      = borg_spell_ratings_MAGE;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_MAGE);
        break;
    case CLASS_DRUID:
        borg_spell_ratings      = borg_spell_ratings_DRUID;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_DRUID);
        break;
    case CLASS_PRIEST:
        borg_spell_ratings      = borg_spell_ratings_PRIEST;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_PRIEST);
        break;
    case CLASS_NECROMANCER:
        borg_spell_ratings      = borg_spell_ratings_NECROMANCER;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_NECROMANCER);
        break;
    case CLASS_PALADIN:
        borg_spell_ratings      = borg_spell_ratings_PALADIN;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_PALADIN);
        break;
    case CLASS_ROGUE:
        borg_spell_ratings      = borg_spell_ratings_ROGUE;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_ROGUE);
        break;
    case CLASS_RANGER:
        borg_spell_ratings      = borg_spell_ratings_RANGER;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_RANGER);
        break;
    case CLASS_BLACKGUARD:
        borg_spell_ratings      = borg_spell_ratings_BLACKGUARD;
        borg_spell_rating_count = N_ELEMENTS(borg_spell_ratings_BLACKGUARD);
        break;
    default:
        /*
         * A class the borg has no ratings for (ZangbandTK, BRG-09).
         *
         * Upstream returned here, *before* `borg_magics` was allocated, and
         * without raising `borg_init_failure`. So the five classes this game
         * added -- Monk, Mindcrafter, Chaos-Warrior, Warrior-Mage, High-Mage
         * -- left `borg_magics` NULL and the borg dereferenced it the first
         * time it thought about casting. No warning, no failure flag, a
         * segfault several hundred turns later.
         *
         * Falling through is the fix. With no table, every spell is unrated
         * and gets the treatment in `borg_init_spell()`: described from the
         * game's data, middling, and without a tactic. The borg plays such a
         * class as a fighter who occasionally casts, which is worse than
         * playing it well and enormously better than crashing.
         *
         * This is the defect M7 would have caught had the borg been running,
         * which is the argument for keeping it running.
         */
        borg_spell_ratings      = NULL;
        borg_spell_rating_count = 0;
        borg_note(format("# No borg spell ratings for %s; every spell is "
                         "unrated", player->class->name));
        break;
    }

    /*
     * ZangbandTK (BRG-10): refuse to run rather than corrupt memory.
     *
     * `borg.book_idx[]` and `borg.amt_book[]` are fixed at BORG_MAX_BOOKS.
     * They are generous now, but the bound they replaced was generous for
     * Angband too and this game outgrew it silently -- a Mage's 28 books
     * against nine slots, with no warning and no crash, just a corrupted
     * structure and a caster that never learned a spell. A clear startup
     * failure is the right answer to the next such change.
     */
    if (player->class->magic.num_books > BORG_MAX_BOOKS) {
        borg_note(format("**STARTUP FAILURE** %s has %d books and the borg "
                         "has room for %d",
            player->class->name, player->class->magic.num_books,
            (int) BORG_MAX_BOOKS));
        borg_init_failure = true;
        return;
    }

    if (borg_magics)
        mem_free(borg_magics);

    borg_magics = NULL;

    /*
     * A class with no spells at all allocates nothing, and must not: the
     * Warrior has no books, and the Mindcrafter's psionics are a power list
     * rather than a realm (PLR-06), so both arrive here with
     * `total_spells == 0`. `mem_zalloc(0)` is not somewhere to index.
     */
    if (player->class->magic.total_spells <= 0) return;

    borg_magics
        = mem_zalloc(player->class->magic.total_spells * sizeof(borg_magic));

    for (int spell = 0; spell < player->class->magic.total_spells; spell++) {
        borg_init_spell(borg_magics, spell);
    }
}

#endif
