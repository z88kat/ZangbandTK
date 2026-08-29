/**
 * \file mon-speech.h
 * \brief What a monster says (ZangbandTK, CNT-04)
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

#ifndef MON_SPEECH_H
#define MON_SPEECH_H

#include "monster.h"

/**
 * One pool of lines: everything a speaker might say in a given situation.
 */
struct monster_speech_pool {
	char **line;
	int count;
};

/**
 * The four pools, from monster_speech.txt.
 */
struct monster_speech {
	struct monster_speech_pool speak;
	struct monster_speech_pool fear;
	struct monster_speech_pool death;
	struct monster_speech_pool crime;
};

extern struct monster_speech mon_speech;

struct monster_speech_pool *monster_speech_pool(struct monster_speech *s,
												const char *key);
void monster_speech_free(struct monster_speech *s);

const char *monster_speech_line(const struct monster_speech_pool *pool);

void monster_speak(struct monster *mon);
void monster_speak_death(struct monster *mon, const char *name);
void monster_claim_bounty(struct player *p, struct monster *mon,
						  const char *name);

#endif /* MON_SPEECH_H */
