/**
 * \file list-virtues.h
 * \brief The eighteen virtues a character may be measured against
 *
 * Zangband's own list, in Zangband's own order
 * ([avatar.c:23](../archive/zangband/src/avatar.c#L23)). The order is the
 * savefile's: a character's virtues are stored as indices into this table, so
 * entries are appended and never inserted or reordered.
 *
 * Note the eighth. Its constant is `V_ENCHANT` throughout Zangband's source
 * and the player is never shown that word -- the table calls it Mysticism, and
 * so does everything the player reads. Both names are kept here rather than
 * one being tidied away, because the code and the documentation each use a
 * different one and a reader has to be able to get from either to the other.
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

/* symbol			display name */
V(COMPASSION,		"Compassion")
V(HONOUR,			"Honour")
V(JUSTICE,			"Justice")
V(SACRIFICE,		"Sacrifice")
V(KNOWLEDGE,		"Knowledge")
V(FAITH,			"Faith")
V(ENLIGHTEN,		"Enlightenment")
V(ENCHANT,			"Mysticism")
V(CHANCE,			"Chance")
V(NATURE,			"Nature")
V(HARMONY,			"Harmony")
V(VITALITY,			"Vitality")
V(UNLIFE,			"Unlife")
V(PATIENCE,			"Patience")
V(TEMPERANCE,		"Temperance")
V(DILIGENCE,		"Diligence")
V(VALOUR,			"Valour")
V(INDIVIDUALISM,	"Individualism")
