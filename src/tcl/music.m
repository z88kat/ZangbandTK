/**
 * \file tcl/music.m
 * \brief The player's own music, for the Tk front end's Sound page.
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
 * OBS-33: music from a folder of the player's own, which ships nothing and so
 * licenses nothing.  This plays one track and answers questions about it;
 * which track, in what order, and when to move on is music.tcl's, which asks
 * now and again whether the one playing has ended.
 *
 * Through AVAudioEngine, which plays whatever Core Audio reads: MP3, AAC,
 * Apple Lossless, WAV, AIFF, CAF and FLAC, and Ogg Vorbis and Opus, the
 * formats free music most often comes in.  Both simpler players were tried
 * first and refuse those two: NSSound will not open them, and AVAudioPlayer
 * opens them and then will not play them.
 *
 * Built on the Mac, beside the Cocoa sound module the game's effects use; see
 * TCL_Frontend.cmake.  Compiled with ARC.
 */
#import <AVFoundation/AVFoundation.h>

#include <stdatomic.h>
#include <stdbool.h>

bool tcl_music_play(const char *path, double volume);
void tcl_music_stop(void);
void tcl_music_pause(bool pause);
void tcl_music_volume(double volume);
const char *tcl_music_state(double *position, double *length);

static AVAudioEngine *engine;
static AVAudioPlayerNode *node;
static AVAudioFile *file;
static bool paused;
static double last_position;

/*
 * The engine says a track is done from a thread of its own, and says it too
 * when a track is stopped or replaced; so each track is a generation, and
 * only the current one's end counts.
 */
static atomic_int generation;
static atomic_int ended;

/* Play a file from the start at a volume from 0 to 1; false if it cannot be. */
bool tcl_music_play(const char *path, double volume)
{
	NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
	int mine;

	tcl_music_stop();
	file = [[AVAudioFile alloc] initForReading:url error:nil];
	if (!file) return false;

	if (!engine) {
		engine = [[AVAudioEngine alloc] init];
		node = [[AVAudioPlayerNode alloc] init];
		[engine attachNode:node];
	}
	[engine stop];
	[engine disconnectNodeOutput:node];
	[engine connect:node to:engine.mainMixerNode format:file.processingFormat];
	node.volume = (float)volume;

	mine = atomic_fetch_add(&generation, 1) + 1;
	atomic_store(&ended, 0);
	[node scheduleFile:file atTime:nil completionHandler:^{
		if (atomic_load(&generation) == mine) atomic_store(&ended, 1);
	}];
	if (![engine startAndReturnError:nil]) {
		file = nil;
		return false;
	}
	[node play];
	last_position = 0;
	return true;
}

void tcl_music_stop(void)
{
	atomic_fetch_add(&generation, 1);
	if (node) [node stop];
	file = nil;
	paused = false;
	atomic_store(&ended, 0);
	last_position = 0;
}

void tcl_music_pause(bool pause)
{
	if (!file || pause == paused) return;
	if (pause) {
		[node pause];
	} else {
		if (!engine.isRunning) [engine startAndReturnError:nil];
		[node play];
	}
	paused = pause;
}

void tcl_music_volume(double volume)
{
	if (node) node.volume = (float)volume;
}

/*
 * "stopped" with nothing loaded, "paused", "playing", or "ended" once the
 * track has played out -- or once the engine has stopped of its own accord,
 * as it does when the output device changes, so that the next track starts it
 * again; and where it is in it, in seconds.
 */
const char *tcl_music_state(double *position, double *length)
{
	AVAudioTime *at;

	*position = 0;
	*length = 0;
	if (!file) return "stopped";
	*length = (double)file.length / file.processingFormat.sampleRate;
	at = [node playerTimeForNodeTime:node.lastRenderTime];
	if (at && at.sampleRate > 0) last_position = at.sampleTime / at.sampleRate;
	*position = last_position;
	if (paused) return "paused";
	if (atomic_load(&ended) || !engine.isRunning) return "ended";
	return "playing";
}
