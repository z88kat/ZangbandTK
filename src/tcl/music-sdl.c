/**
 * \file tcl/music-sdl.c
 * \brief The player's own music, for the Tk front end on Windows and Linux.
 *
 * Copyright (c) 2026 Steven Pannell
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

/*
 * music.m's counterpart where the game's sounds come through SDL2 (snd-sdl.c):
 * the same five functions, so main-tcl.c and music.tcl neither know nor care
 * which one they have.  One track at a time, on SDL_mixer's music stream,
 * which the game's own sounds no longer use -- they are chunks, mixed on the
 * channels beside it.
 *
 * SDL_mixer plays MP3, Ogg Vorbis, Opus, FLAC, WAV and AIFF; not AAC, which
 * music.tcl therefore does not offer here.  The mixer is the one snd-sdl.c
 * opened at startup; with no sound module running there is nothing to play
 * through, and play says so.
 *
 * How far into a track, and how long it is, SDL_mixer can say from 2.6 on.
 * Before that -- Ubuntu 22.04 has 2.0.4, and the Linux build is made there for
 * the glibc it asks for -- the length is not known and is given as 0, and the
 * position is the time played, counted here.
 */
#include <stdbool.h>

#include "SDL.h"
#include "SDL_mixer.h"

bool tcl_music_play(const char *path, double volume);
void tcl_music_stop(void);
void tcl_music_pause(bool pause);
void tcl_music_volume(double volume);
const char *tcl_music_state(double *position, double *length);

#ifdef SDL_MIXER_VERSION_ATLEAST
#if SDL_MIXER_VERSION_ATLEAST(2, 6, 0)
#define MUSIC_HAS_TIMES
#endif
#endif

static Mix_Music *music;
static bool paused;

/* The time played, for an SDL_mixer that cannot say: ticks, less the pauses. */
static Uint32 started;
static Uint32 paused_at;
static Uint32 paused_for;

static int mix_volume(double volume)
{
	if (volume < 0) volume = 0;
	if (volume > 1) volume = 1;
	return (int)(volume * MIX_MAX_VOLUME + 0.5);
}

/* Play a file from the start at a volume from 0 to 1; false if it cannot be. */
bool tcl_music_play(const char *path, double volume)
{
	tcl_music_stop();

	/* No mixer, no music: the sound module did not start. */
	if (!Mix_QuerySpec(NULL, NULL, NULL)) return false;

	music = Mix_LoadMUS(path);
	if (!music) return false;

	Mix_VolumeMusic(mix_volume(volume));
	if (Mix_PlayMusic(music, 1) < 0) {
		Mix_FreeMusic(music);
		music = NULL;
		return false;
	}
	started = SDL_GetTicks();
	paused_for = 0;
	return true;
}

void tcl_music_stop(void)
{
	if (music) {
		Mix_HaltMusic();
		Mix_FreeMusic(music);
		music = NULL;
	}
	paused = false;
}

void tcl_music_pause(bool pause)
{
	if (!music || pause == paused) return;
	if (pause) {
		Mix_PauseMusic();
		paused_at = SDL_GetTicks();
	} else {
		Mix_ResumeMusic();
		paused_for += SDL_GetTicks() - paused_at;
	}
	paused = pause;
}

void tcl_music_volume(double volume)
{
	if (music) Mix_VolumeMusic(mix_volume(volume));
}

/*
 * "stopped" with nothing loaded, "paused", "playing", or "ended" once the
 * track has played out; and where it is in it, and its length, in seconds --
 * the length 0 where this SDL_mixer cannot tell.
 */
const char *tcl_music_state(double *position, double *length)
{
	*position = 0;
	*length = 0;
	if (!music) return "stopped";

#ifdef MUSIC_HAS_TIMES
	*position = Mix_GetMusicPosition(music);
	*length = Mix_MusicDuration(music);
	if (*position < 0) *position = 0;
	if (*length < 0) *length = 0;
#else
	{
		Uint32 now = paused ? paused_at : SDL_GetTicks();

		*position = (now - started - paused_for) / 1000.0;
	}
#endif

	if (paused) return "paused";
	if (!Mix_PlayingMusic()) return "ended";
	return "playing";
}
