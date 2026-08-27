/**
 * \file buildid.c
 * \brief Compile in build details
 *
 * Copyright (c) 2011 Andi Sidwell
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

#include "buildid.h"

/*
 * VERSION_STRING is ours, and comes from buildid.h.  Upstream took it from a
 * generated version.h or from the BUILD_ID macro, both of which resolve to a
 * `git describe` of the source tree; see buildid.h for why that is not what a
 * player should be shown.  Both build systems still define BUILD_ID and nothing
 * now reads it.
 */

const char *buildid = VERSION_NAME " " VERSION_STRING;
const char *buildver = VERSION_STRING;

/**
 * Link a copyright message into the executable
 */
/*
 * The credit lines, in one place because two things want them: `copyright`
 * below, which is the whole notice, and `copyright_brief`, which is these lines
 * alone for the About dialogs -- they have room for a few lines and not for the
 * licence.  Written twice, they would drift, and the one that drifts is the one
 * nobody reads until it is wrong in front of a player.
 */
#define COPYRIGHT_LINES \
	"Copyright (c) 1987-2026 Angband contributors.\n" \
	"Copyright (c) 1997-2005 Zangband DevTeam.\n" \
	"Copyright (c) 2026 ZangbandTK contributors."

const char *copyright_brief = COPYRIGHT_LINES;

/*
 * Where to find the project.  The About dialogs used to name Angband's own
 * homepage and forum, which is where an unbranded variant sends its players by
 * default and exactly the wrong place to send ours.
 */
const char *homepage = "https://zangbandtk.com";

const char *copyright =
	COPYRIGHT_LINES
	"\n"
	"\n"
	"ZangbandTK " VERSION_STRING " is a variant of Angband " BASE_VERSION_STRING ",\n"
	"rebuilding the spirit of Zangband on a modern base.  It incorporates\n"
	"material from Zangband, which is available under the Angband licence\n"
	"only.\n"
	"\n"
	"This work is free software; you can redistribute it and/or modify it\n"
	"under the terms of either:\n"
	"\n"
	"a) the GNU General Public License as published by the Free Software\n"
	"   Foundation, version 2, or\n"
	"\n"
	"b) the Angband licence:\n"
	"   This software may be copied and distributed for educational, research,\n"
	"   and not for profit purposes provided that this copyright and statement\n"
	"   are included in all such copies.  Other copyrights may also apply.\n";
