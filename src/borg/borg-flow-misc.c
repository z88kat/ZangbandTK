/**
 * \file borg-flow-misc.c
 * \brief Misc movement (flow) routines
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

#include "../dun-type.h"
#include "../wild.h"
#include "borg-flow-misc.h"
#include "borg-prepared.h"

#ifdef ALLOW_BORG

#include "../cave.h"
#include "../store.h"
#include "../ui-term.h"

#include "borg-cave-light.h"
#include "borg-cave-util.h"
#include "borg-cave-view.h"
#include "borg-cave.h"
#include "borg-danger.h"
#include "borg-escape.h"
#include "borg-flow-kill.h"
#include "borg-flow-stairs.h"
#include "borg-flow.h"
#include "borg-io.h"
#include "borg-item-activation.h"
#include "borg-item-use.h"
#include "borg-item-val.h"
#include "borg-magic.h"
#include "borg-projection.h"
#include "borg-trait.h"
#include "borg-update.h"
#include "borg.h"

/*
 * Locate the store doors
 */
int *track_shop_x;
int *track_shop_y;

/*
 * Track the mineral veins with treasure
 */
struct borg_track track_vein;

/*
 * Do a "reverse" flow from the player outwards
 */
void borg_flow_reverse(int depth, bool optimize, bool avoid, bool tunneling,
    int stair_idx, bool sneak)
{
    /* Clear the flow codes */
    borg_flow_clear();

    /* Enqueue the player's grid */
    borg_flow_enqueue_grid(borg.c.y, borg.c.x);

    /* Spread, but do NOT optimize */
    borg_flow_spread(depth, optimize, avoid, tunneling, stair_idx, sneak);
}

/* 
 * Get the borgs "leash"
 * This is the distance from the stairs the borg can explore before 
 * returning to the stairs and trying to explore in anohter direction.
 * The leash is different for exploring vs trying to get something.
 */
int borg_get_leash(bool pick_up)
{
    int leash = 250;

    if (pick_up && borg.trait[BI_CLEVEL] < 20)
        leash = borg.trait[BI_CLEVEL] * 3 + 9;

    if (!pick_up && borg.trait[BI_CDEPTH] >= borg.trait[BI_CLEVEL] - 5)
        leash = borg.trait[BI_CLEVEL] * 3 + 9;

    /* if the borg has run out of things to do, allow him to go a */
    /* little further afield */
    if (borg.times_twitch > 21)
        leash += borg.times_twitch;

    return leash;
}

/*
 * Check a floor grid for "happy" status
 *
 * These grids are floor grids which contain stairs, or which
 * are non-corners in corridors, or which are directly adjacent
 * to pillars, or grids which we have stepped on before.
 *  Stairs are good because they can be used to leave
 * the level.  Corridors are good because you can back into them
 * to avoid groups of monsters and because they can be used for
 * escaping.  Pillars are good because while standing next to a
 * pillar, you can walk "around" it in two different directions,
 * allowing you to retreat from a single normal monster forever.
 * Stepped on grids are good because they likely stem from an area
 * which has been cleared of monsters.
 */
bool borg_happy_grid_bold(int y, int x)
{
    int i;

    borg_grid *ag = &borg_grids[y][x];

    /*
     * Bounds Check
     *
     * ZangbandTK (BRG-12): against the level, not against the dungeon.
     *
     * `DUNGEON_HGT` is Angband's 66 and every level there is the dungeon. On
     * this game's wilderness surface -- 144 x 144 as the data ships -- a
     * character starts at about row 81, so this returned false for every grid
     * the player was ever standing on and the borg believed the whole surface
     * was the outside wall of the world. That is why it could not find a
     * town's shops, could not buy food, and stair-scummed between the surface
     * and depth 1 for three thousand turns saying "unable to dive: restock
     * food < 3".
     */
    if (y >= cave->height - 2 || y <= 2 || x >= cave->width - 2 || x <= 2)
        return false;

    /* Accept stairs */
    if (ag->feat == FEAT_LESS)
        return true;
    if (ag->feat == FEAT_MORE)
        return true;
    if (ag->glyph)
        return true;
    if (ag->feat == FEAT_LAVA && !borg.trait[BI_IFIRE])
        return false;

    /* Weak/dark is very unhappy */
    if (borg.trait[BI_ISWEAK] || borg.trait[BI_LIGHT] == 0)
        return false;

    /* Apply a control effect so that he does not get stuck in a loop */
    if ((borg_t - borg_began) >= 2000)
        return false;

    /* Case 1a: north-south corridor */
    if (borg_cave_floor_bold(y - 1, x) && borg_cave_floor_bold(y + 1, x)
        && !borg_cave_floor_bold(y, x - 1) && !borg_cave_floor_bold(y, x + 1)
        && !borg_cave_floor_bold(y + 1, x - 1)
        && !borg_cave_floor_bold(y + 1, x + 1)
        && !borg_cave_floor_bold(y - 1, x - 1)
        && !borg_cave_floor_bold(y - 1, x + 1)) {
        /* Happy */
        return true;
    }

    /* Case 1b: east-west corridor */
    if (borg_cave_floor_bold(y, x - 1) && borg_cave_floor_bold(y, x + 1)
        && !borg_cave_floor_bold(y - 1, x) && !borg_cave_floor_bold(y + 1, x)
        && !borg_cave_floor_bold(y + 1, x - 1)
        && !borg_cave_floor_bold(y + 1, x + 1)
        && !borg_cave_floor_bold(y - 1, x - 1)
        && !borg_cave_floor_bold(y - 1, x + 1)) {
        /* Happy */
        return true;
    }

    /* Case 1aa: north-south doorway */
    if (borg_cave_floor_bold(y - 1, x) && borg_cave_floor_bold(y + 1, x)
        && !borg_cave_floor_bold(y, x - 1) && !borg_cave_floor_bold(y, x + 1)) {
        /* Happy */
        return true;
    }

    /* Case 1ba: east-west doorway */
    if (borg_cave_floor_bold(y, x - 1) && borg_cave_floor_bold(y, x + 1)
        && !borg_cave_floor_bold(y - 1, x) && !borg_cave_floor_bold(y + 1, x)) {
        /* Happy */
        return true;
    }

    /* Case 2a: north pillar */
    if (!borg_cave_floor_bold(y - 1, x) && borg_cave_floor_bold(y - 1, x - 1)
        && borg_cave_floor_bold(y - 1, x + 1)
        && borg_cave_floor_bold(y - 2, x)) {
        /* Happy */
        return true;
    }

    /* Case 2b: south pillar */
    if (!borg_cave_floor_bold(y + 1, x) && borg_cave_floor_bold(y + 1, x - 1)
        && borg_cave_floor_bold(y + 1, x + 1)
        && borg_cave_floor_bold(y + 2, x)) {
        /* Happy */
        return true;
    }

    /* Case 2c: east pillar */
    if (!borg_cave_floor_bold(y, x + 1) && borg_cave_floor_bold(y - 1, x + 1)
        && borg_cave_floor_bold(y + 1, x + 1)
        && borg_cave_floor_bold(y, x + 2)) {
        /* Happy */
        return true;
    }

    /* Case 2d: west pillar */
    if (!borg_cave_floor_bold(y, x - 1) && borg_cave_floor_bold(y - 1, x - 1)
        && borg_cave_floor_bold(y + 1, x - 1)
        && borg_cave_floor_bold(y, x - 2)) {
        /* Happy */
        return true;
    }

    /* check for grids that have been stepped on before */
    for (i = 0; i < track_step.num; i++) {
        /* Enqueue the grid */
        if ((track_step.y[i] == y) && (track_step.x[i] == x)) {
            /* Recent step is good */
            if (i < 25) {
                return true;
            }
        }
    }

    /* Not happy */
    return false;
}

/*
 * Attempt to flow to a safe grid in order to rest up properly.  Following a
 * battle, a borg needs to heal up. He will attempt to heal up right where the
 * fight was, but if he cannot, then he needs to retreat a bit. This will help
 * him find a good safe place to hide.
 */
bool borg_flow_recover(int dist)
{
    int i, x, y;

    /* Sometimes we loop on this */
    if (borg.time_this_panel > 500)
        return false;

    /* No retreating and recovering when low level */
    if (borg.trait[BI_CLEVEL] <= 5)
        return false;

    /* Mana for spell casters */
    if (borg_primarily_caster()) {
        if (borg.trait[BI_CURHP] > borg.trait[BI_MAXHP] / 3
            && ((borg.trait[BI_CURSP] > borg.trait[BI_MAXSP] / 4)
                || borg.trait[BI_MAXSP] == 0)
            && /* Non spell casters? */
            !borg.trait[BI_ISCUT] && !borg.trait[BI_ISSTUN]
            && !borg.trait[BI_ISHEAVYSTUN] && !borg.trait[BI_ISAFRAID])
            return false;
    } else /* Non Spell Casters */
    {
        /* do I need to recover some? */
        if (borg.trait[BI_CURHP] > borg.trait[BI_MAXHP] / 3
            && !borg.trait[BI_ISCUT] && !borg.trait[BI_ISSTUN]
            && !borg.trait[BI_ISHEAVYSTUN] && !borg.trait[BI_ISAFRAID])
            return false;
    }

    /* If Fleeing, then do not rest */
    if (borg.goal.fleeing)
        return false;

    /* If Scumming, then do not rest */
    if (borg.lunal_mode || borg.munchkin_mode)
        return false;

    /* No need if hungry */
    if (borg.trait[BI_ISHUNGRY])
        return false;

    /* Nothing found */
    borg_temp_n = 0;

    /* Scan some known Grids
     * Favor the following types of grids:
     * 1. Happy grids
     */

    /* look at grids within 20 grids of me */
    for (y = borg.c.y - 25; y < borg.c.y + 25; y++) {

        for (x = borg.c.x - 25; x < borg.c.x + 25; x++) {
            /* Stay in bounds */
            if (!square_in_bounds(cave, loc(x, y)))
                continue;

            /* Skip my own grid */
            if (y == borg.c.y && x == borg.c.x)
                continue;

            /* Skip grids that are too close to me */
            if (distance(borg.c, loc(x, y)) < 7)
                continue;

            /* Is this grid a happy grid? */
            if (!borg_happy_grid_bold(y, x))
                continue;

            /* Can't rest on a wall grid. */
            /* HACK depends on FEAT order, kinda evil */
            if (borg_grids[y][x].feat >= FEAT_SECRET
                && borg_grids[y][x].feat != FEAT_PASS_RUBBLE)
                continue;

            /* Can I rest on that one? */
            if (!borg_check_rest(y, x))
                continue;

            /* Careful -- Remember it */
            borg_temp_x[borg_temp_n] = x;
            borg_temp_y[borg_temp_n] = y;
            borg_temp_n++;
        }
    }

    /* Nothing to kill */
    if (!borg_temp_n)
        return false;

    /* Clear the flow codes */
    borg_flow_clear();

    /* Look through the good grids */
    for (i = 0; i < borg_temp_n; i++) {
        /* Enqueue the grid */
        borg_flow_enqueue_grid(borg_temp_y[i], borg_temp_x[i]);
    }

    /* Spread the flow */
    borg_flow_spread(dist, false, true, false, -1, false);

    /* Attempt to Commit the flow */
    if (!borg_flow_commit("Recover Grid", GOAL_RECOVER))
        return false;

    /* Take one step */
    if (!borg_flow_old(GOAL_RECOVER))
        return false;

    return true;
}

/*
 * Prepare to "flow" towards mineral veins with treasure
 */
bool borg_flow_vein(bool viewable, int nearness)
{
    int i, x, y;
    int b_stair = -1, j, b_j = -1;
    int cost  = 0;
    int leash = borg_get_leash(true);
    uint8_t min_feat;

    borg_grid *ag;

    /* Efficiency -- Nothing to take */
    if (!track_vein.num)
        return false;

    /* Not needed if rich */
    if (borg.trait[BI_GOLD] >= 100000)
        return false;

    /* Require digger, capacity, or skill to dig  */
    /* note, if twitchy we will try digging out magma */
    min_feat = FEAT_QUARTZ_K;
    if (borg.times_twitch > 21)
        min_feat = FEAT_MAGMA_K;
    if (!borg_can_dig(true, min_feat))
        return false;

    /* Nothing yet */
    borg_temp_n = 0;

    /* Check distance away from stairs, used later */
    /* Check for an existing "up stairs" */
    for (i = 0; i < track_less.num; i++) {
        x = track_less.x[i];
        y = track_less.y[i];

        /* How far is the nearest up stairs */
        j = distance(borg.c, loc(x, y));

        /* skip the closer ones */
        if (b_j >= j)
            continue;

        /* track it */
        b_j     = j;
        b_stair = i;
    }

    /* Scan the vein list */
    for (i = 0; i < track_vein.num; i++) {
        /* Access the location */
        x = track_vein.x[i];
        y = track_vein.y[i];

        /* Get the grid */
        ag = &borg_grids[y][x];

        /* Require line of sight if requested */
        if (viewable && !(ag->info & BORG_VIEW))
            continue;

        /* Clear the flow codes */
        borg_flow_clear();

        /* obtain the number of steps from this take to the stairs */
        if (nearness > 5 && borg.trait[BI_CLEVEL] < 20) {
            cost = borg_flow_cost_stair(y, x, b_stair);

            /* Check the distance to stair for this proposed grid, unless i am
             * looking for very close items (leash) */
            if (cost > leash)
                continue;
        }
        /* Careful -- Remember it */
        borg_temp_x[borg_temp_n] = x;
        borg_temp_y[borg_temp_n] = y;
        borg_temp_n++;
    }

    /* Nothing to mine */
    if (!borg_temp_n)
        return false;

    /* Clear the flow codes */
    borg_flow_clear();

    /* Look for something to take */
    for (i = 0; i < borg_temp_n; i++) {
        /* Enqueue the grid */
        borg_flow_enqueue_grid(borg_temp_y[i], borg_temp_x[i]);
    }

    /* Spread the flow */
    /* if we are not flowing toward items that we can see, make sure they */
    /* are at least easily reachable.  The second flag is weather or not  */
    /* to avoid unkown squares.  This was for performance. */
    borg_flow_spread(nearness, true, !viewable, false, -1, false);

    /* Attempt to Commit the flow */
    if (!borg_flow_commit("vein", GOAL_TAKE))
        return false;

    /* Take one step */
    if (!borg_flow_old(GOAL_TAKE))
        return false;

    /* Success */
    return true;
}

/*
 * Spastic searching
 */

static uint8_t spastic_x;
static uint8_t spastic_y;

/*
 * Search carefully for secret doors and such
 */
bool borg_flow_spastic(bool bored)
{
    int cost;

    int i, x, y, v;

    int b_x = borg.c.x;
    int b_y = borg.c.y;
    int b_v = -1;
    int j, b_j = -1;
    int b_stair = -1;

    borg_grid *ag;

    /* Not in town */
    if (!borg.trait[BI_CDEPTH])
        return false;

    /* Not if starving */
    if (borg.trait[BI_ISWEAK])
        return false;

    /* Not if hopeless unless twitchy */
    if (borg_t - borg_began > 3000 && avoidance <= borg.trait[BI_CURHP])
        return false;

    /* Not bored */
    if (!bored) {
        /* Look around for danger */
        int p = borg_danger(borg.c.y, borg.c.x, 1, true, false);

        /* Avoid searching when in danger */
        if (p > avoidance / 4)
            return false;
    }

    /* Check distance away from stairs, used later */
    /* Check for an existing "up stairs" */
    for (i = 0; i < track_less.num; i++) {
        x = track_less.x[i];
        y = track_less.y[i];

        /* How far is the nearest up stairs */
        j = distance(borg.c, loc(x, y));

        /* skip the closer ones */
        if (b_j >= j)
            continue;

        /* track it */
        b_j     = j;
        b_stair = i;
    }

    /* We have arrived */
    if ((spastic_x == borg.c.x) && (spastic_y == borg.c.y)) {
        /* Cancel */
        spastic_x = 0;
        spastic_y = 0;

        ag        = &borg_grids[borg.c.y][borg.c.x];

        /* Take note */
        borg_note(format("# Spastic Searching at (%d,%d)...value:%d", borg.c.x,
            borg.c.y, ag->xtra));

        /* Count searching */
        for (i = 0; i < 9; i++) {
            /* Extract the location */
            int xx = borg.c.x + ddx_ddd[i];
            int yy = borg.c.y + ddy_ddd[i];

            /* Current grid */
            ag = &borg_grids[yy][xx];

            /* Tweak -- Remember the search */
            if (ag->xtra < 100)
                ag->xtra += 5;
        }

        /* we searched here */
        return false;
    }

    /* Reverse flow */
    borg_flow_reverse(250, true, false, false, -1, false);

    /* Scan the entire map */
    for (y = 1; y < AUTO_MAX_Y - 1; y++) {
        for (x = 1; x < AUTO_MAX_X - 1; x++) {
            borg_grid *ag_ptr[8];

            int wall     = 0;
            int supp     = 0;
            int diag     = 0;
            int monsters = 0;

            /* Acquire the grid */
            ag = &borg_grids[y][x];

            /* Skip unknown grids */
            if (ag->feat == FEAT_NONE)
                continue;

            /* Skip trap grids */
            if (ag->trap)
                continue;

            /* Skip walls/doors */
            if (!borg_cave_floor_grid(ag))
                continue;

            /* Acquire the cost */
            cost = borg_data_cost->data[y][x];

            /* Skip "unreachable" grids */
            if (cost >= 250)
                continue;

            /* Skip grids that are really far away.  He probably
             * won't find anything and it takes lots of turns
             */
            if (cost >= 25 && borg.trait[BI_CLEVEL] < 30)
                continue;
            if (cost >= 50)
                continue;

            /* Tweak -- Limit total searches */
            if (ag->xtra >= 50)
                continue;
            if (ag->xtra >= borg.trait[BI_CLEVEL])
                continue;

            /* Limit initial searches until bored */
            if (!bored && (ag->xtra > 5))
                continue;

            /* Avoid searching detected sectors */
            if (borg_detect_door[y / borg_panel_hgt()][x / borg_panel_wid()])
                continue;

            /* Skip ones that make me wander too far unless twitchy (Leash)*/
            if (b_stair != -1 && borg.trait[BI_CLEVEL] < 15
                && avoidance <= borg.trait[BI_CURHP]) {
                /* Check the distance of this grid to the stair */
                j = borg_distance(
                    track_less.y[b_stair], track_less.x[b_stair], y, x);
                /* Distance of me to the stairs */
                b_j = borg_distance(borg.c.y, borg.c.x, track_less.y[b_stair],
                    track_less.x[b_stair]);

                /* skip far away grids while I am close to stair*/
                if (b_j <= borg.trait[BI_CLEVEL] * 3 + 9
                    && j >= borg.trait[BI_CLEVEL] * 3 + 9)
                    continue;

                /* If really low level don't do this much */
                if (borg.trait[BI_CLEVEL] <= 3
                    && b_j <= borg.trait[BI_CLEVEL] + 9
                    && j >= borg.trait[BI_CLEVEL] + 9)
                    continue;

                /* Do not Venture too far from stair */
                if (borg.trait[BI_CLEVEL] <= 3
                    && j >= borg.trait[BI_CLEVEL] + 5)
                    continue;

                /* Do not Venture too far from stair */
                if (borg.trait[BI_CLEVEL] <= 10
                    && j >= borg.trait[BI_CLEVEL] + 9)
                    continue;
            }

            /* Extract adjacent locations */
            for (i = 0; i < 8; i++) {
                /* Extract the location */
                int xx = x + ddx_ddd[i];
                int yy = y + ddy_ddd[i];

                /* Get the grid contents */
                ag_ptr[i] = &borg_grids[yy][xx];
            }

            /* Count possible door locations */
            for (i = 0; i < 4; i++) {
                ag = ag_ptr[i];
                if (ag->feat >= FEAT_GRANITE)
                    wall++;
            }

            /* No possible secret doors */
            if (wall < 1)
                continue;

            /* Count supporting evidence for secret doors */
            for (i = 0; i < 4; i++) {
                ag = ag_ptr[i];

                /* Rubble */
                if (ag->feat == FEAT_RUBBLE)
                    continue;

                /* Walls, Doors */
                if (((ag->feat >= FEAT_SECRET) && (ag->feat <= FEAT_GRANITE))
                    || ((ag->feat == FEAT_OPEN) || (ag->feat == FEAT_BROKEN))
                    || (ag->feat == FEAT_CLOSED)) {
                    supp++;
                }
            }

            /* Count supporting evidence for secret doors */
            for (i = 4; i < 8; i++) {
                ag = ag_ptr[i];

                /* Rubble */
                if (ag->feat == FEAT_RUBBLE)
                    continue;

                /* Walls */
                if (ag->feat >= FEAT_SECRET) {
                    diag++;
                }
            }

            /* No possible secret doors */
            if (diag < 2)
                continue;

            /* Count monsters */
            for (i = 0; i < 8; i++) {
                ag = ag_ptr[i];

                /* monster */
                if (ag->kill)
                    monsters++;
            }

            /* No search near monsters */
            if (monsters >= 1)
                continue;

            /* Tweak -- Reward walls, punish visitation, distance, time on level
             */
            v = (supp * 500) + (diag * 100) - (ag->xtra * 40) - (cost * 2)
                - (borg_t - borg_began);

            /* Punish low level and searching too much */
            v -= (50 - borg.trait[BI_CLEVEL]) * 5;

            /* The grid is not searchable */
            if (v <= 0)
                continue;

            /* Tweak -- Minimal interest until bored */
            if (!bored && (v < 1500))
                continue;

            /* Track "best" grid */
            if ((b_v >= 0) && (v < b_v))
                continue;

            /* Save the data */
            b_v = v;
            b_x = x;
            b_y = y;
        }
    }

    /* Clear the flow codes */
    borg_flow_clear();

    /* Nothing found */
    if (b_v < 0)
        return false;

    /* Access grid */
    ag = &borg_grids[b_y][b_x];

    /* Memorize */
    spastic_x = b_x;
    spastic_y = b_y;

    /* Enqueue the grid */
    borg_flow_enqueue_grid(b_y, b_x);

    /* Spread the flow */
    borg_flow_spread(250, true, false, false, -1, false);

    /* Attempt to Commit the flow */
    if (!borg_flow_commit("spastic", GOAL_XTRA))
        return false;

    /* Take one step */
    if (!borg_flow_old(GOAL_XTRA))
        return false;

    /* Success */
    return true;
}

/*
 * Prepare to "flow" towards a specific shop entry
 */
/*
 * Cross the world to the mouth of a deeper dungeon (ZangbandTK, BRG-13).
 *
 * There is no route to depth 30 without this. The Vaults of Amber, which the
 * town staircase leads into, ends at depth 15; `player_dungeon_at_stairs()`
 * always sends a town staircase to the shallowest dungeon there is; and not
 * one of the thirteen dungeon mouths is inside the starting 144x144 window --
 * the nearest reaching past 15 is 576 grids away, and the world is roughly
 * fourteen windows by fourteen.
 *
 * The design is a bearing rather than a route, and the measurement is what
 * licenses that. Sampling the straight line to every mouth at block
 * resolution, across four seeds: **at least one dungeon whose band reaches
 * past 15 always has a completely clear line**, usually several, and Rebma
 * (band 25-50, which contains the target depth) was clear in all four. Where a
 * line is blocked it is `mountainside`, never open sea. So the borg prefers a
 * target it does not have to route around, which turns a pathfinding problem
 * into a target-selection one.
 *
 * The goal is held in **world** coordinates. The surface window is rebuilt and
 * re-anchored as the character crosses it (`wild_adopt_window()`), so a level
 * grid stops meaning anything the moment the window moves; a world grid
 * survives it.
 */

/* How many steps a crossing gets before it is abandoned as hopeless. */
#define BORG_WORLD_TRIES 400

/* Does this mouth sit on the road network?  (ZangbandTK, BRG-13.) */
static bool borg_mouth_on_road(struct wild_dungeon *mouth)
{
    int size = z_info->wild_block_size;

    if (!wild || size < 1) return false;

    return wild_road_at(wild, mouth->grid.x / size, mouth->grid.y / size);
}

/*
 * Is the straight line from here to this mouth clear of impassable terrain?
 *
 * Sampled per wilderness block, which is the granularity at which the terrain
 * actually varies. Town and dungeon blocks are markers rather than walls --
 * `wild_block_feat()` returns FEAT_PERM for one and FEAT_DUNGEON for the other
 * and a character walks through both -- and counting them as obstacles made
 * every line look blocked on the first measurement.
 */
static bool borg_world_line_clear(struct wild_dungeon *mouth)
{
    int size = z_info->wild_block_size;
    int fy, fx, ty, tx, steps, k;

    if (!wild || size < 1) return false;

    fy = (player->grid.y + player->wild_offset.y) / size;
    fx = (player->grid.x + player->wild_offset.x) / size;
    ty = mouth->grid.y / size;
    tx = mouth->grid.x / size;

    steps = MAX(ABS(ty - fy), ABS(tx - fx));
    if (steps < 1) return true;

    for (k = 0; k <= steps; k++) {
        int by   = fy + (ty - fy) * k / steps;
        int bx   = fx + (tx - fx) * k / steps;
        int feat = wild_block_feat(wild, bx, by);

        if (feat == FEAT_PERM || feat == FEAT_DUNGEON) continue;
        if (feat == FEAT_WATER) continue;   /* crossable at the edges */
        if (!feat_is_passable(feat)) return false;
    }

    return true;
}

/*
 * Choose a mouth worth walking to, or -1.
 *
 * Wanted: a dungeon that reaches deeper than the one we are stuck in, that we
 * are ready to enter at its shallowest level, and whose line is clear.
 * Nearest first among those, because every grid walked is a turn not spent
 * descending.
 */
static int borg_choose_dungeon(void)
{
    int i, n, best = -1, best_dist = 0;
    int here_floor = 0;

    if (!wild) return -1;

    /*
     * Only cross when actually stuck.
     *
     * Without this the borg walks the world on its first turn: with no
     * dungeon visited yet there is no floor to be stuck at, so every dungeon
     * counts as "deeper than here" and the nearest one wins -- which was
     * measured as an 831-grid hike to the mouth of the Vaults of Amber while
     * the town staircase into the very same dungeon stood ten grids away, and
     * a death at character level 1 in open country for its trouble.
     *
     * So: it must have been somewhere, and it must have reached that
     * somewhere's bottom. Anything else is a reason to take the stairs it
     * already has.
     */
    if (!player->dungeon) return -1;

    {
        const struct dun_type *t = dun_type_by_index(player->dungeon - 1);

        if (!t) return -1;
        here_floor = t->max_depth;
    }

    if (borg.trait[BI_MAXDEPTH] < here_floor) return -1;

    n = wild_dungeon_count(wild);

    for (i = 0; i < n; i++) {
        struct wild_dungeon  *m = wild_dungeon_by_index(wild, i);
        const struct dun_type *t;
        int dist;

        if (!m) continue;
        if (i == borg.goal.world_best) continue;  /* already given up on */

        t = dun_type_by_index(m->type);
        if (!t) continue;

        /* Has to go deeper than where we are stuck */
        if (t->max_depth <= here_floor) continue;

        /*
         * And we have to survive arriving. Entering a mouth puts the
         * character at that dungeon's shallowest level, and the borg's own
         * rule refuses a depth above its character level -- so a dungeon
         * starting at 25 is no use at character level 6, however clear the
         * road to it.
         */
        if (t->min_depth > borg.trait[BI_MAXCLEVEL]) continue;

        /*
         * A road to the door beats a clear line to it (ZangbandTK, BRG-13).
         *
         * The project owner's design, and it is what the game was built for:
         * *"The borg should follow the road, that's what most players will
         * do."* `wild_place_roads()` lays a spanning tree over the towns, the
         * short hops between neighbours, and then **a spur to every dungeon
         * mouth** -- its own comment records that six of thirteen mouths
         * happened to sit on a road before that pass existed and the rest were
         * up to a thousand grids of open country away.
         *
         * So a road is better than a measured line in three ways. It is
         * passable by construction, which retires the terrain question rather
         * than sampling around it. It avoids the worst country, because
         * `wild_road_cost()` routes around mountains, so following one is how
         * a character survives the crossing. And it is legible in the log:
         * "walked the road to Rebma" can be diagnosed, a bearing that wandered
         * cannot.
         *
         * The clear-line test is kept as the fallback for a mouth with no road
         * to it, which has not been observed but is not guaranteed.
         */
        if (!borg_mouth_on_road(m) && !borg_world_line_clear(m)) continue;

        dist = ABS(m->grid.y - (player->grid.y + player->wild_offset.y))
             + ABS(m->grid.x - (player->grid.x + player->wild_offset.x));

        if (best < 0 || dist < best_dist) {
            best      = i;
            best_dist = dist;
        }
    }

    return best;
}

/*
 * Which town the borg is standing in, or -1 out in the country.
 */
static int borg_town_here(void)
{
    struct loc w;

    if (!wild || !player) return -1;

    w.y = player->grid.y + player->wild_offset.y;
    w.x = player->grid.x + player->wild_offset.x;

    return wild_town_here(wild, w);
}

/*
 * What the borg can buy where it stands, as a store bitmask.
 *
 * Standing in a town, that town's trades. Standing anywhere else, nothing --
 * and the "nothing" is the point (ZangbandTK, BRG-25).
 *
 * Written first as "the nearest town", on the reasoning that the borg spends
 * its surface time in the country and the comparison wants the town whose
 * shops it uses. Measured, that is wrong in the way that matters: a borg 334
 * grids from Avalon was credited with Avalon's magic shop, decided it wanted
 * for nothing, and stood in a field. Nearest is not reachable, and a shop the
 * borg has never seen is a shop it cannot buy from -- it knew four shops all
 * run, and all four were in the village it left.
 *
 * Out of town the borg has no shops at all, so anything it wants is a reason
 * to walk to a town that has it. That is also what makes the wandering
 * purposeful: `borg_flow_dark()` will explore a scrolling wilderness for ever,
 * and this is the borg having somewhere to be instead.
 */
static uint16_t borg_stores_at_hand(void)
{
    int i;
    struct loc w;

    /*
     * Slack around the walls, because the wall is not where the shops stop
     * being reachable.
     *
     * `wild_town_here()` is exact, which is right for "have I arrived" and
     * wrong for "can I shop". Measured with the exact test: the borg stepped
     * one grid outside Avalon, found itself with no shops in reach, targeted
     * Avalon one grid away, stepped back in, announced its arrival, and did it
     * again -- fifty-one arrivals in a single run. A borg a dozen grids from
     * the gate is at the town for every purpose that matters here.
     */
    const int slack = 12;

    if (!wild || !player) return 0;

    w.y = player->grid.y + player->wild_offset.y;
    w.x = player->grid.x + player->wild_offset.x;

    for (i = 0; i < wild_town_count(wild); i++) {
        struct loc        org = wild_town_origin_of(wild, i);
        struct wild_town *t   = &wild->towns[i];

        if (w.x >= org.x - slack && w.x < org.x + t->wid + slack
            && w.y >= org.y - slack && w.y < org.y + t->hgt + slack)
            return t->stores;
    }

    return 0;
}

/*
 * The town the borg is in, or the nearest -- for reporting only.
 */
int borg_town_home(void)
{
    int i, home, best_dist = 0;

    if (!wild || !player) return -1;

    home = borg_town_here();
    if (home >= 0) return home;

    for (i = 0; i < wild_town_count(wild); i++) {
        struct loc org = wild_town_origin_of(wild, i);
        int dist = ABS(org.y - (player->grid.y + player->wild_offset.y))
                 + ABS(org.x - (player->grid.x + player->wild_offset.x));

        if (home < 0 || dist < best_dist) {
            home      = i;
            best_dist = dist;
        }
    }

    return home;
}

/*
 * Which trades could end the shortfall (ZangbandTK, BRG-25).
 *
 * Read from the same traits `borg_restock()` counts, rather than from the
 * string it returns or by asking it. Two reasons, and the second was measured.
 *
 * Matching on the message would work today and break the first time somebody
 * rewords one, and a borg that silently stops travelling because a message
 * changed is a bug nobody would think to look for.
 *
 * And calling `borg_prepared()` to locate the wall -- which is the obvious
 * way to write this, and how it was written first -- is not safe from here.
 * It is asked once per turn from the flow, it walks up to forty depths, and
 * it is not a pure query: it settles `borg.ready_morgoth` on the way past and
 * returns a pointer into a shared static buffer. Speculating with it wedged a
 * cheated Warrior on an unexpected direction prompt at turn 787,049, and the
 * same run without the speculation reached the time cap cleanly. Traits are
 * plain reads and cannot do that.
 *
 * The thresholds are the deepest band's rather than the current one's, because
 * this decides whether a shop is worth a walk of several hundred grids, not
 * whether to buy something standing in front of it. Arriving with two teleport
 * sources when six are wanted at depth 36 is a second crossing later.
 */
uint16_t borg_stores_wanted(void)
{
    uint16_t want = 0;

    /* Food, light and fuel: the general store */
    if (borg.trait[BI_FOOD] < 5 || borg.trait[BI_LIGHT] < 2
        || (borg.trait[BI_AFUEL] < 5 && !borg.trait[BI_LIGHT]))
        want |= 1u << WILD_STORE_GENERAL;

    /* Phase door, cure wounds, word of recall: the alchemist */
    if (borg.trait[BI_APHASE] < 2 || borg.trait[BI_RECALL] < 2
        || borg.trait[BI_ACLW] + borg.trait[BI_ACSW] + borg.trait[BI_ACCW] < 4)
        want |= 1u << WILD_STORE_ALCHEMY;

    /*
     * Teleportation: the magic shop, and the one that actually bites. A Scroll
     * of Teleportation is stocked nowhere in town -- only `normal:staff:
     * Teleportation` in the magic shop -- so this requirement cannot be met at
     * all in a place without one, whatever the borg is carrying in gold.
     *
     * Not wanted until the borg has been deep enough for the wall to be in
     * sight. `borg_restock()` first demands two teleport sources at depth 10,
     * so a character that has reached 8 is two levels from being stopped and
     * has survived enough country to cross some. Wanting it from the first
     * turn is BRG-13's mistake in another costume: that was an 831-grid hike
     * at character level one, and it died in a field.
     */
    if (borg.trait[BI_MAXDEPTH] >= 8
        && borg.trait[BI_ATELEPORT] + borg.trait[BI_AESCAPE] < 6)
        want |= 1u << WILD_STORE_MAGIC;

    return want;
}

/*
 * Choose a town worth walking to, or -1 (ZangbandTK, BRG-25).
 *
 * The starting village keeps a general store, a bookseller, an alchemist and
 * the home, and nothing else, because WLD-11a makes the armoury, the
 * weaponsmith, the magic shop and the black market the reason a larger town is
 * worth the walk. That is deliberate design and it is also a wall: the borg's
 * own restock rule wants two teleport sources from depth 10, the only town
 * source is the magic shop's Staff of Teleportation, and the village has no
 * magic shop. Measured, the borg stopped at depth 9 with 206,000 gold.
 *
 * So: only when short of something buyable, and only towards a town that keeps
 * a trade this one does not. Nearest first among those, and a road preferred
 * over open country for the same three reasons the dungeon crossing prefers
 * one -- passable by construction, routed around the worst land, and legible
 * afterwards.
 */
int borg_choose_town(void)
{
    int here, i, best = -1, best_dist = 0;
    uint16_t have, want;

    if (!wild || !player) return -1;

    /*
     * The town the borg shops at, which is the nearest one rather than the one
     * underfoot (ZangbandTK, BRG-25).
     *
     * Requiring it to be standing inside the walls looked right and was
     * measured wrong: the borg spends its surface time in the country between
     * the town and the dungeon it dives into, so `wild_town_here()` answers -1
     * almost every time the question is asked. The comparison wants the town
     * whose shops it can actually use, and that is the nearest, whether or not
     * it happens to be inside the gate this turn.
     */
    here = borg_town_here();
    have = borg_stores_at_hand();

    /*
     * And only for a trade that would actually help. "Any shop this one has
     * not got" was measured sending the borg a thousand grids to the only
     * black market in the world while it wanted a staff from the magic shop
     * two hundred grids the other way.
     */
    want = borg_stores_wanted() & ~have;
    if (!want) return -1;

    for (i = 0; i < wild_town_count(wild); i++) {
        struct wild_town *t = &wild->towns[i];
        struct loc        org;
        int               dist;

        if (i == here) continue;

        /* Must keep a trade the borg needs and cannot buy where it stands */
        if (!(t->stores & want)) continue;

        org = wild_town_origin_of(wild, i);

        dist = ABS(org.y - (player->grid.y + player->wild_offset.y))
             + ABS(org.x - (player->grid.x + player->wild_offset.x));

        if (best < 0 || dist < best_dist) {
            best      = i;
            best_dist = dist;
        }
    }

    return best;
}

/*
 * Walk toward the chosen mouth, one step per call. True if a step was taken.
 *
 * Inside the window this is an ordinary flow to a known grid. Outside it, the
 * borg walks toward the bearing and lets the window rebuild around it, which
 * is why the goal is kept in world coordinates.
 */
bool borg_flow_world(void)
{
    int ly, lx, dist;

    /* Only on the surface, and only when there is somewhere better to be */
    if (borg.trait[BI_CDEPTH]) return false;
    if (!wild || !cave) return false;

    /* Pick a target, or keep the one we have */
    if (borg.goal.world_kind == BORG_WORLD_NONE) {
        int pick = borg_choose_dungeon();

        if (pick >= 0) {
            struct wild_dungeon   *m = wild_dungeon_by_index(wild, pick);
            const struct dun_type *t = m ? dun_type_by_index(m->type) : NULL;

            if (!m || !t) return false;

            borg.goal.world_kind  = BORG_WORLD_DUNGEON;
            borg.goal.world_index = pick;
            borg.goal.world       = m->grid;

            borg_note(format("# Crossing the world to %s (depth %d-%d), "
                             "%d grids away",
                t->name, t->min_depth, t->max_depth,
                ABS(m->grid.y - (player->grid.y + player->wild_offset.y))
                + ABS(m->grid.x - (player->grid.x + player->wild_offset.x))));
        } else {
            /*
             * Nowhere deeper to walk to, so try somewhere better stocked
             * (ZangbandTK, BRG-25). Second, not first: a dungeon that goes
             * deeper is always the better use of the walk, and shopping is
             * what the borg falls back on when depth is what it cannot buy.
             */
            pick = borg_choose_town();

            if (pick < 0) return false;

            borg.goal.world_kind  = BORG_WORLD_TOWN;
            borg.goal.world_index = pick;
            borg.goal.world       = wild_town_origin_of(wild, pick);

            borg_note(format("# Crossing the world to %s for its shops "
                             "(want %04x), %d grids away",
                wild->towns[pick].name ? wild->towns[pick].name : "a town",
                (unsigned) borg_stores_wanted(),
                ABS(borg.goal.world.y
                    - (player->grid.y + player->wild_offset.y))
                + ABS(borg.goal.world.x
                    - (player->grid.x + player->wild_offset.x))));
        }

        borg.goal.world_tries = BORG_WORLD_TRIES;
        borg.goal.world_best  = -1;
    }

    /*
     * Keep the goal current. A mouth's grid never moves, but re-reading it
     * each step means a target that has gone away -- a bad index, a world
     * rebuilt underneath -- abandons the walk rather than walking to a stale
     * coordinate.
     */
    if (borg.goal.world_kind == BORG_WORLD_DUNGEON) {
        struct wild_dungeon *m
            = wild_dungeon_by_index(wild, borg.goal.world_index);

        if (!m) {
            borg.goal.world_kind = BORG_WORLD_NONE;
            return false;
        }
        borg.goal.world = m->grid;
    } else if (borg.goal.world_index < 0
               || borg.goal.world_index >= wild_town_count(wild)) {
        borg.goal.world_kind = BORG_WORLD_NONE;
        return false;
    }

    /* Where the goal sits in the window as it is now anchored */
    ly = borg.goal.world.y - player->wild_offset.y;
    lx = borg.goal.world.x - player->wild_offset.x;

    dist = ABS(borg.goal.world.y - (player->grid.y + player->wild_offset.y))
         + ABS(borg.goal.world.x - (player->grid.x + player->wild_offset.x));

    /*
     * A town is arrived at by being inside it, not by standing on one grid of
     * it. Its origin is a corner, and the shops are spread across a rectangle
     * up to 132 grids wide -- walking to the corner and calling that arrival
     * would leave the borg outside the wall of a place it had crossed the
     * world to reach.
     */
    if (borg.goal.world_kind == BORG_WORLD_TOWN
        && borg_town_here() == borg.goal.world_index) {
        borg_note(format("# Arrived at %s; shopping from here",
            wild->towns[borg.goal.world_index].name
                ? wild->towns[borg.goal.world_index].name : "the town"));
        borg.goal.world_kind = BORG_WORLD_NONE;
        return false;
    }

    /*
     * The step budget, and it is spent on *failing to close the distance*
     * rather than on steps taken. A walk that is getting nearer may take as
     * long as it likes; one that is not is abandoned, and the mouth is
     * remembered as hopeless so the next choice is a different one. Retrying
     * the same unreachable target every time the borg gets bored is the same
     * thrash as the stair-scum loop.
     */
    if (borg.goal.world_best < 0 || dist < borg.goal.world_best) {
        borg.goal.world_best  = dist;
        borg.goal.world_tries = BORG_WORLD_TRIES;
    } else if (--borg.goal.world_tries <= 0) {
        borg_note(format("# Giving up on the crossing; %d grids short", dist));
        borg.goal.world_best = borg.goal.world_index; /* remember it */
        borg.goal.world_kind = BORG_WORLD_NONE;
        return false;
    }

    /* Arrived: step onto the mouth and the descent does the rest */
    if (ly == borg.c.y && lx == borg.c.x) {
        if (borg.goal.world_kind == BORG_WORLD_DUNGEON) {
            borg_note("# Standing on the mouth of the dungeon we chose");
            borg.goal.world_kind = BORG_WORLD_NONE;
            borg_keypress('>');
            return true;
        }

        /* A town corner reached without being counted as inside it */
        borg.goal.world_kind = BORG_WORLD_NONE;
        return false;
    }

    /* In the window: an ordinary flow to a known grid */
    if (ly >= 0 && ly < cave->height && lx >= 0 && lx < cave->width) {
        borg_flow_clear();
        borg_flow_enqueue_grid(ly, lx);
        borg_flow_spread(250, true, false, false, -1, false);

        if (borg_flow_commit(borg.goal.world_kind == BORG_WORLD_TOWN
                                 ? "the town we chose"
                                 : "the dungeon we chose",
                             GOAL_MISC))
            return borg_flow_old(GOAL_MISC);
    }

    /*
     * Outside the window: walk the bearing and let the world scroll.
     *
     * All eight directions are considered, ranked by how much each closes the
     * world-space distance, so a step round an obstacle is a step sideways
     * rather than a stop. Deep water is refused rather than routed around: a
     * borg at low character level cannot swim, and drowning itself is a worse
     * outcome than abandoning the crossing -- which only costs it the depth it
     * already had.
     */
    {
        int wy = player->grid.y + player->wild_offset.y;
        int wx = player->grid.x + player->wild_offset.x;
        int best_dir = 0, best_gain = 0, i;

        for (i = 0; i < 8; i++) {
            struct loc g = loc(borg.c.x + ddx_ddd[i], borg.c.y + ddy_ddd[i]);
            int gain, dir;

            if (!square_in_bounds_fully(cave, g)) continue;
            if (!square_ispassable(cave, g)) continue;
            if (square_iswater(cave, g)) continue;

            /* How much nearer this step leaves us, in world grids */
            gain = dist
                - (ABS(borg.goal.world.y - (wy + ddy_ddd[i]))
                   + ABS(borg.goal.world.x - (wx + ddx_ddd[i])));

            if (gain <= 0) continue;

            /*
             * Weight a step that stays on the road (ZangbandTK, BRG-13).
             *
             * Roads run from every town to every mouth and route around the
             * mountains, so a step along one is both passable and safer than
             * the same step across open country. Weighted rather than
             * required: a road is a block-resolution fact and the character
             * moves grid by grid, so insisting on it would stall at every
             * block edge. Two grids' worth of preference is enough to hold
             * the road where one exists and to be overridden by a genuinely
             * shorter way where it does not.
             */
            if (z_info->wild_block_size > 0
                && wild_road_at(wild,
                                (wx + ddx_ddd[i]) / z_info->wild_block_size,
                                (wy + ddy_ddd[i]) / z_info->wild_block_size)) {
                gain += 2;
            }

            dir = borg_extract_dir(borg.c.y, borg.c.x,
                                   borg.c.y + ddy_ddd[i],
                                   borg.c.x + ddx_ddd[i]);
            if (!dir) continue;

            if (gain > best_gain) {
                best_gain = gain;
                best_dir  = dir;
            }
        }

        if (best_dir) {
            borg_keypress(I2D(best_dir));
            return true;
        }
    }

    return false;
}

bool borg_flow_shop_entry(int i)
{
    int x, y;

    const char *name = (f_info[stores[i].feat].name);

    /* Must be in town */
    if (borg.trait[BI_CDEPTH])
        return false;

    /* Obtain the location */
    x = track_shop_x[i];
    y = track_shop_y[i];

    /* Ignore shop with unset (zero) coordinates */
    if (!x || !y)
        return false;

    /* Re-enter a shop if needed */
    if ((x == borg.c.x) && (y == borg.c.y)) {
        /* Note */
        borg_note("# Re-entering a shop");

        /* Enter the store */
        borg_keypress('5');

        /* Success */
        return true;
    }

    /* Clear the flow codes */
    borg_flow_clear();

    /* Enqueue the grid */
    borg_flow_enqueue_grid(y, x);

    /* Spread the flow */
    borg_flow_spread(250, true, false, false, -1, false);

    /* Attempt to Commit the flow */
    if (!borg_flow_commit(name, GOAL_MISC))
        return false;

    /* Take one step */
    if (!borg_flow_old(GOAL_MISC))
        return false;

    /* Success */
    return true;
}

/*
 * Prepare to flow towards light
 */
bool borg_flow_light(int why)
{
    int y, x, i;

    /* reset counters */
    borg_glow_n = 0;
    i           = 0;

    /* build the glow array */
    /* Scan map */
    for (y = w_y; y < w_y + SCREEN_HGT; y++) {
        for (x = w_x; x < w_x + SCREEN_WID; x++) {
            borg_grid *ag = &borg_grids[y][x];

            /* Not a perma-lit, and not our spot. */
            if (!(ag->info & BORG_GLOW))
                continue;

            /* keep count */
            borg_glow_y[borg_glow_n] = y;
            borg_glow_x[borg_glow_n] = x;
            borg_glow_n++;
        }
    }
    /* None to flow to */
    if (!borg_glow_n)
        return false;

    /* Clear the flow codes */
    borg_flow_clear();

    /* Enqueue useful grids */
    for (i = 0; i < borg_glow_n; i++) {
        /* Enqueue the grid */
        borg_flow_enqueue_grid(borg_glow_y[i], borg_glow_x[i]);
    }

    /* Spread the flow */
    borg_flow_spread(250, true, false, false, -1, false);

    /* Attempt to Commit the flow */
    if (!borg_flow_commit("a lighted area", why))
        return false;

    /* Take one step */
    if (!borg_flow_old(why))
        return false;

    /* Success */
    return true;
}

/*
 * Prepare to flow towards a vault grid which can be excavated
 */
bool borg_flow_vault(int nearness)
{
    int  y, x, i;
    int  b_y, b_x;
    bool can_dig_hard;

    borg_grid *ag;

    /* reset counters */
    borg_temp_n = 0;
    i           = 0;

    /* no need if no vault on level */
    if (!vault_on_level)
        return false;

    /* no need if we can't dig at least quartz */
    if (!borg_can_dig(false, FEAT_QUARTZ))
        return false;

    can_dig_hard = borg_can_dig(false, FEAT_GRANITE);

    /* build the array -- Scan screen */
    for (y = w_y; y < w_y + SCREEN_HGT; y++) {
        for (x = w_x; x < w_x + SCREEN_WID; x++) {

            /* only bother with near ones */
            if (distance(borg.c, loc(x, y)) > nearness)
                continue;

            uint8_t feat = borg_grids[y][x].feat;

            /* only deal with excavatable walls */
            if (feat != FEAT_RUBBLE
                && feat != FEAT_QUARTZ 
                && feat != FEAT_MAGMA
                && feat != FEAT_QUARTZ_K 
                && feat != FEAT_MAGMA_K) {
                /* only deal with granite if we are good diggers */
                if (!can_dig_hard || feat != FEAT_GRANITE)
                    continue;
            }

            /* Examine grids adjacent to this grid to see if there is a perma
             * wall adjacent */
            for (i = 0; i < 8; i++) {
                b_x = x + ddx_ddd[i];
                b_y = y + ddy_ddd[i];

                /* Bounds check */
                if (!square_in_bounds_fully(cave, loc(b_x, b_y)))
                    continue;

                /* Access the grid */
                ag = &borg_grids[b_y][b_x];

                /* Not a perma, and not our spot. */
                if (ag->feat != FEAT_PERM)
                    continue;

                /* keep count */
                borg_temp_y[borg_temp_n] = y;
                borg_temp_x[borg_temp_n] = x;
                borg_temp_n++;
            }
        }
    }

    /* None to flow to */
    if (!borg_temp_n)
        return false;

    /* Examine each ones */
    for (i = 0; i < borg_temp_n; i++) {
        /* Enqueue the grid */
        borg_flow_enqueue_grid(borg_temp_y[i], borg_temp_x[i]);
    }

    /* Spread the flow */
    borg_flow_spread(250, true, false, false, -1, false);

    /* Attempt to Commit the flow */
    if (!borg_flow_commit("vault excavation", GOAL_VAULT))
        return false;

    /* Take one step */
    if (!borg_flow_old(GOAL_VAULT))
        return false;

    /* Success */
    return true;
}

/*
 * Act twitchy
 */
bool borg_twitchy(void)
{
    int dir = 5;
    int count;
    bool all_walls = true;
    borg_grid * grid = NULL;
    struct loc l;

    /* This is a bad thing */
    borg_note("# Twitchy!");

    /* try to phase out of it */
    if (borg_allow_teleport()) {
        if (borg_caution_phase(15, 2)
            && (borg_spell_fail(PHASE_DOOR, 40) || borg_spell_fail(PORTAL, 40)
                || borg_shadow_shift(40) || borg_activate_item(act_tele_phase)
                || borg_activate_item(act_tele_long)
                || borg_read_scroll(sv_scroll_phase_door))) {
            /* We did something */
            return true;
        }
    }

    /* Pick a random direction */
    count = 20;
    while (true) {
        dir = randint0(10);
        if (dir == 5 || dir == 0)
            continue;

        if (!count)
            break;

        count--;

        /* Set goal */
        borg.goal.g.x = borg.c.x + ddx[dir];
        borg.goal.g.y = borg.c.y + ddy[dir];

        if (!square_in_bounds_fully(cave, borg.goal.g))
            continue;

        grid = &borg_grids[borg.goal.g.y][borg.goal.g.x];

        /* don't twitch into walls */
        if (grid->feat >= FEAT_SECRET && grid->feat <= FEAT_PERM)
            continue;

        /* monsters count as walls if afraid */
        if (grid->kill && borg.trait[BI_ISAFRAID])
            continue;

        break;
    }

    /* if we can't find a direction, maybe we are surrounded by walls */
    if (!count) {
        for (dir = 1; dir < 10; dir++) {
            if (dir == 5)
                continue;

            /* get the location of position + direction */
            l.x = borg.c.x + ddx[dir];
            l.y = borg.c.y + ddy[dir];

            if (!square_in_bounds_fully(cave, l))
                continue;

            grid = &borg_grids[l.y][l.x];

            /* don't twitch into walls */
            if (grid->feat >= FEAT_SECRET && grid->feat <= FEAT_PERM) {
                /* unless afraid (and even then, not perm walls) */
                if (!borg.trait[BI_ISAFRAID] || grid->feat == FEAT_PERM)
                    continue;
            }

            /* monsters count as walls if afraid */
            if (grid->kill && borg.trait[BI_ISAFRAID])
                continue;

            all_walls = false;
            break;
        }
        if (all_walls) {
            /* Rest until done */
            borg_keypress('R');
            borg_keypress('1');
            borg_keypress('0');
            borg_keypress('0');
            borg_keypress(KC_ENTER);
            /* We did something */
            return true;
        }
    }

    /* Normally move */

    /* if afraid, we need to try to dig. We have tried everything else */
    /* digging will at least take an action so maybe give time to not be */
    /* afraid */
    if (borg.trait[BI_ISAFRAID])
        borg_keypress('+');

    /* Send direction */
    borg_keypress(I2D(dir));

    /* We did something */
    return true;
}

/*
 * Given a "source" and "target" locations, extract a "direction",
 * which will move one step from the "source" towards the "target".
 *
 * Note that we use "diagonal" motion whenever possible.
 *
 * We return "5" if no motion is needed.
 */
int borg_extract_dir(int y1, int x1, int y2, int x2)
{
    /* No movement required */
    if ((y1 == y2) && (x1 == x2))
        return 5;

    /* South or North */
    if (x1 == x2)
        return ((y1 < y2) ? 2 : 8);

    /* East or West */
    if (y1 == y2)
        return ((x1 < x2) ? 6 : 4);

    /* South-east or South-west */
    if (y1 < y2)
        return ((x1 < x2) ? 3 : 1);

    /* North-east or North-west */
    if (y1 > y2)
        return ((x1 < x2) ? 9 : 7);

    /* Paranoia */
    return 5;
}

/*
 * Given a "source" and "target" locations, travel in a "direction",
 * which will move one step from the "source" towards the "target".
 *
 * We prefer "non-diagonal" motion, which allows us to save the
 * "diagonal" moves for avoiding pillars and other obstacles.
 *
 * If no "obvious" path is available, we use "borg_extract_dir()".
 *
 * We return "5" if no motion is needed.
 */
int borg_goto_dir(int y1, int x1, int y2, int x2)
{
    int d, e;

    int ay = (y2 > y1) ? (y2 - y1) : (y1 - y2);
    int ax = (x2 > x1) ? (x2 - x1) : (x1 - x2);

    /* Default direction */
    e = borg_extract_dir(y1, x1, y2, x2);

    /* Adjacent location, use default */
    if ((ax <= 1) && (ay <= 1))
        return (e);

    /* Try south/north (primary) */
    if (ay > ax) {
        d = (y1 < y2) ? 2 : 8;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;
    }

    /* Try east/west (primary) */
    if (ay < ax) {
        d = (x1 < x2) ? 6 : 4;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;
    }

    /* Try diagonal */
    d = borg_extract_dir(y1, x1, y2, x2);

    /* Check for walls */
    if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
        return d;

    /* Try south/north (secondary) */
    if (ay <= ax) {
        d = (y1 < y2) ? 2 : 8;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;
    }

    /* Try east/west (secondary) */
    if (ay >= ax) {
        d = (x1 < x2) ? 6 : 4;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;
    }

    /* Circle obstacles */
    if (!ay) {
        /* Circle to the south */
        d = (x1 < x2) ? 3 : 1;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;

        /* Circle to the north */
        d = (x1 < x2) ? 9 : 7;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;
    }

    /* Circle obstacles */
    if (!ax) {
        /* Circle to the east */
        d = (y1 < y2) ? 3 : 9;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;

        /* Circle to the west */
        d = (y1 < y2) ? 1 : 7;
        if (borg_cave_floor_bold(y1 + ddy[d], x1 + ddx[d]))
            return d;
    }

    /* Oops */
    return (e);
}

/*
 * check to make sure there are no monsters around that should prevent resting
 */
bool borg_check_rest(int y, int x)
{
    int  i, ii;
    bool borg_in_vault = false;

    /* never rest to recover SP (if HP at max) if you only recover */
    /* sp in combat */
    if (borg.trait[BI_CURHP] == borg.trait[BI_MAXHP]
        && player_has(player, PF_COMBAT_REGEN))
        return false;

    /* Do not rest recently after killing a multiplier */
    /* This will avoid the problem of resting next to */
    /* an unkown area full of breeders */
    if (borg.when_last_kill_mult > (borg_t - 4)
        && borg.when_last_kill_mult <= borg_t)
        return false;

    /* No resting if Blessed and good HP and good SP */
    /* don't rest for SP if you do combat regen */
    if ((borg.temp.bless || borg.temp.hero || borg.temp.berserk
            || borg.temp.fastcast || borg.temp.regen || borg.temp.smite_evil)
        && !borg.munchkin_mode
        && (borg.trait[BI_CURHP] >= borg.trait[BI_MAXHP] * 8 / 10)
        && (borg.trait[BI_CURSP] >= borg.trait[BI_MAXSP] * 7 / 10))
        return false;

    /* Set this to Zero */
    borg.when_last_kill_mult = 0;

    /* Most of the time, its ok to rest in a vault */
    if (vault_on_level) {
        for (i = -1; i < 1; i++) {
            for (ii = -1; ii < 1; ii++) {
                /* check bounds */
                if (!square_in_bounds_fully(
                        cave, loc(borg.c.x + ii, borg.c.y + i)))
                    continue;

                if (borg_grids[borg.c.y + i][borg.c.x + ii].feat == FEAT_PERM)
                    borg_in_vault = true;
            }
        }
    }

    /* No resting to recover if I just cast a prepatory spell
     * which is what I like to do right before I take a stair,
     * Unless I am down by three quarters of my SP.
     */
    if (borg.no_rest_prep >= 1 && !borg.munchkin_mode
        && borg.trait[BI_CURSP] > borg.trait[BI_MAXSP] / 4
        && borg.trait[BI_CDEPTH] < 85)
        return false;

    /* Don't rest on lava unless we are immune to fire */
    if (borg_grids[y][x].feat == FEAT_LAVA && !borg.trait[BI_IFIRE])
        return false;

    /* Dont worry about fears if in a vault */
    if (!borg_in_vault) {
        /* Be concerned about the Regional Fear. */
        if (borg_fear_region[y / 11][x / 11] > borg.trait[BI_CURHP] / 20
            && borg.trait[BI_CDEPTH] != 100)
            return false;

        /* Be concerned about the Monster Fear. */
        if (borg_fear_monsters[y][x] > borg.trait[BI_CURHP] / 10
            && borg.trait[BI_CDEPTH] != 100)
            return false;

        /* Be concerned about the Monster Danger. */
        if (borg_danger(y, x, 1, true, false) > borg.trait[BI_CURHP] / 40
            && borg.trait[BI_CDEPTH] >= 85)
            return false;

        /* Be concerned if low on food */
        if ((borg.trait[BI_LIGHT] == 0 || borg.trait[BI_ISWEAK]
                || borg.trait[BI_FOOD] < 2)
            && !borg.munchkin_mode)
            return false;
    }

    /* Examine all the monsters */
    for (i = 1; i < borg_kills_nxt; i++) {
        borg_kill           *kill  = &borg_kills[i];
        struct monster_race *r_ptr = &r_info[kill->r_idx];

        int x9                     = kill->pos.x;
        int y9                     = kill->pos.y;
        int ax, ay, d;
        int p = 0;

        /* Skip dead monsters */
        if (!kill->r_idx)
            continue;

        /* Distance components */
        ax = (x9 > x) ? (x9 - x) : (x - x9);
        ay = (y9 > y) ? (y9 - y) : (y - y9);

        /* Distance */
        d = MAX(ax, ay);

        /* Minimal distance */
        if (d > z_info->max_range)
            continue;

        /* if too close to a Mold or other Never-Mover, don't rest */
        if (d < 2 && !(rf_has(r_ptr->flags, RF_NEVER_MOVE)))
            return false;
        if (d == 1)
            return false;

        /* if too close to a Multiplier, don't rest */
        if (d < 10 && (rf_has(r_ptr->flags, RF_MULTIPLY)))
            return false;

        /* If monster is asleep, dont worry */
        if (!kill->awake && d > 8 && !borg.munchkin_mode)
            continue;

        /* one call for dangers */
        p = borg_danger_one_kill(y9, x9, 1, i, true, true);

        /* Ignore proximity checks while inside a vault */
        if (!borg_in_vault) {
            /* Real scary guys pretty close */
            if (d < 5 && (p > avoidance / 3) && !borg.munchkin_mode)
                return false;

            /* scary guys far away */
            /*if (d < 17 && d > 5 && (p > avoidance/3)) return false; */
        }

        /* should check LOS... monster to me concerned for Ranged Attacks */
        if (borg_los(y9, x9, y, x) && kill->ranged_attack)
            return false;

        /* Special handling for the munchkin mode */
        if (borg.munchkin_mode && borg_los(y9, x9, y, x)
            && (kill->awake && !(rf_has(r_ptr->flags, RF_NEVER_MOVE))))
            return false;

        /* if it walks through walls, not safe */
        if ((rf_has(r_ptr->flags, RF_PASS_WALL)) && !borg_in_vault)
            return false;
        if (rf_has(r_ptr->flags, RF_KILL_WALL) && !borg_in_vault)
            return false;
    }
    return true;
}

/* check if this spot is too far from the stairs */
bool borg_flow_far_from_stairs(int x, int y, int b_stair)
{
    return borg_flow_far_from_stairs_dist(
        x, y, b_stair, borg_get_leash(false));
}

/* check if this spot is too far from the stairs */
bool borg_flow_far_from_stairs_dist(int x, int y, int b_stair, int distance)
{
    if (borg.trait[BI_CDEPTH] >= borg.trait[BI_CLEVEL] - 5
        && borg.trait[BI_CLEVEL] < 20) {

        /* obtain the number of steps from this take to the stairs */
        int cost = borg_flow_cost_stair(y, x, b_stair);

        /* Check the distance to stair for this proposed grid */
        if (cost > distance)
            return true;
    }

    return false;
}

void borg_init_flow_misc(void)
{
    /* Track the shop locations */
    track_shop_x = mem_zalloc(9 * sizeof(int));
    track_shop_y = mem_zalloc(9 * sizeof(int));

    /* Track mineral veins with treasure. */
    borg_init_track(&track_vein, 100);
}

void borg_free_flow_misc(void)
{
    borg_free_track(&track_vein);

    mem_free(track_shop_y);
    track_shop_y = NULL;
    mem_free(track_shop_x);
    track_shop_x = NULL;
}

#endif
