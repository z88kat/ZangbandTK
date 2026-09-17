/**
 * \file main-tcl.c
 * \brief The Tcl/Tk front end.
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

#include "angband.h"

#ifdef USE_TCL

#include "init.h"
#include "main.h"
#include "ui-term.h"

#include <tcl.h>
#include <tk.h>

/*
 * This is ZangbandTK/Tk -- the front end the whole of Phase 3 is about.  It is
 * a second macOS application beside the Cocoa one rather than a replacement
 * for it; see .claude/plans/phase3-tcl-tk-frontend.md, decision 14.
 *
 * Milestone T0, which is all that is here: embed a Tcl interpreter, bring up
 * Tk, open one window, and let the game's main loop drive it.  The term hooks
 * below are deliberately the smallest thing that keeps ui-term.c happy -- T1
 * gives them a real grid to draw on.  Nothing here draws the map yet.
 *
 * Two structural points that outlive T0:
 *
 *   1. The game keeps its main loop (DEC-14).  Tk's event loop is not entered;
 *      instead Tcl_DoOneEvent is pumped from Term_xtra, which is the seam that
 *      lets a Tk window coexist with a program that was written to block on
 *      getch().  Never call Tk_MainLoop here.
 *
 *   2. The front end links the whole text UI and overrides its hooks one at a
 *      time, after textui_init() has installed them.  That is what makes the
 *      later milestones independent of each other: each native dialog is one
 *      hook reassignment, and anything not yet converted keeps working.
 */

/**
 * The one window, for now.
 */
static Tcl_Interp *interp = NULL;
static Tk_Window mainwin = NULL;

/**
 * A term, and the Tk widget it will eventually draw into.
 */
typedef struct term_data term_data;
struct term_data {
	term t;
};

static term_data td_main;

/**
 * Point Tcl and Tk at their own script libraries.
 *
 * Tcl_Init finds init.tcl by walking up from the executable, which works for a
 * tclsh installed beside its lib directory and does not work for a game
 * executable sitting in a cmake build tree.  Rather than discover that as
 * "invalid command name" from the first script we source, say where they are.
 *
 * The environment wins if it is already set, so a developer can point a build
 * at a different toolchain without reconfiguring.  A bundled .app will carry
 * its own copy and set neither.
 */
static void set_script_library_paths(void)
{
	if (!getenv("TCL_LIBRARY")) {
		static char buf[1024];
		strnfmt(buf, sizeof(buf), "TCL_LIBRARY=%s/lib/tcl9.0",
				TCLTK_PREFIX_PATH);
		putenv(buf);
	}
	if (!getenv("TK_LIBRARY")) {
		static char buf[1024];
		strnfmt(buf, sizeof(buf), "TK_LIBRARY=%s/lib/tk9.0",
				TCLTK_PREFIX_PATH);
		putenv(buf);
	}
}

/**
 * Handle a request from the game to do something outside drawing.
 *
 * TERM_XTRA_EVENT is the important one: it is where the game hands control
 * back, and therefore the only place Tk gets to process anything.  Blocking
 * there when the game asks us to block is what keeps the application from
 * spinning at 100% while the player thinks.
 */
static errr Term_xtra_tcl(int n, int v)
{
	switch (n) {
		case TERM_XTRA_EVENT:
			/* v is true when the game is willing to wait for input. */
			if (v) {
				Tcl_DoOneEvent(TCL_ALL_EVENTS);
			} else {
				while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT))
					;
			}
			return 0;

		case TERM_XTRA_FLUSH:
			while (Tcl_DoOneEvent(TCL_ALL_EVENTS | TCL_DONT_WAIT))
				;
			return 0;

		case TERM_XTRA_DELAY:
			if (v > 0) Tcl_Sleep(v);
			return 0;

		case TERM_XTRA_CLEAR:
		case TERM_XTRA_FRESH:
		case TERM_XTRA_REACT:
		case TERM_XTRA_NOISE:
		case TERM_XTRA_SHAPE:
			/* T1 gives these something to do. */
			return 0;
	}

	return 1;
}

static errr Term_curs_tcl(int x, int y)
{
	(void)x;
	(void)y;
	return 0;
}

static errr Term_wipe_tcl(int x, int y, int n)
{
	(void)x;
	(void)y;
	(void)n;
	return 0;
}

static errr Term_text_tcl(int x, int y, int n, int a, const wchar_t *s)
{
	(void)x;
	(void)y;
	(void)n;
	(void)a;
	(void)s;
	return 0;
}

static void Term_init_tcl(term *t)
{
	(void)t;
}

static void Term_nuke_tcl(term *t)
{
	(void)t;
}

/**
 * Wire one term to the window.
 */
static void term_data_link(term_data *td)
{
	term *t = &td->t;

	term_init(t, 80, 24, 256);

	/* We draw the cursor ourselves; there is no hardware one here. */
	t->soft_cursor = true;

	t->init_hook = Term_init_tcl;
	t->nuke_hook = Term_nuke_tcl;
	t->xtra_hook = Term_xtra_tcl;
	t->curs_hook = Term_curs_tcl;
	t->wipe_hook = Term_wipe_tcl;
	t->text_hook = Term_text_tcl;

	t->data = td;

	Term_activate(t);
	angband_term[0] = t;
}

/**
 * angband_quit -- the Tcl side of closing the window.
 *
 * The game owns the main loop, so a Tk event handler cannot end the program by
 * returning.  quit() unwinds through the game's own shutdown, which is what
 * saves the character, so the window manager's close button routes here.
 *
 * Note the signature: this is Tcl_ObjCmdProc2, which takes Tcl_Size.  Tcl 9
 * keeps the older int-based Tcl_ObjCmdProc and Tcl_CreateObjCommand for
 * compatibility, so a command written the modern way has to be registered with
 * Tcl_CreateObjCommand2 or the prototypes quietly disagree.
 */
static int objcmd_quit(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	(void)dummy;
	(void)ip;
	(void)objc;
	(void)objv;

	quit(NULL);

	/* Not reached. */
	return TCL_OK;
}

const char help_tcl[] = "Tcl/Tk front end";

/**
 * Bring up the interpreter and the window.
 */
errr init_tcl(int argc, char **argv)
{
	(void)argc;

	/*
	 * Tcl insists on knowing where the executable is before an interpreter
	 * exists -- it is how zipfs and the library paths are resolved later.
	 */
	Tcl_FindExecutable(argv[0]);
	set_script_library_paths();

	interp = Tcl_CreateInterp();
	if (!interp) {
		plog("Tcl/Tk: could not create an interpreter.");
		return 1;
	}

	if (Tcl_Init(interp) != TCL_OK) {
		plog_fmt("Tcl/Tk: Tcl_Init failed: %s", Tcl_GetStringResult(interp));
		return 1;
	}

	if (Tk_Init(interp) != TCL_OK) {
		plog_fmt("Tcl/Tk: Tk_Init failed: %s", Tcl_GetStringResult(interp));
		return 1;
	}

	mainwin = Tk_MainWindow(interp);
	if (!mainwin) {
		plog("Tcl/Tk: Tk started but there is no main window.");
		return 1;
	}

	Tcl_CreateObjCommand2(interp, "angband_quit", objcmd_quit, NULL, NULL);

	/*
	 * A placeholder, so that T0 shows something honest rather than an empty
	 * grey rectangle that could equally be a bug.  T1 replaces the body of
	 * this window with the term grid.
	 */
	if (Tcl_Eval(interp,
			/*
			 * An unbundled Tk application on Aqua opens a Tcl console
			 * window of its own and puts it in front of everything -- the
			 * first concrete instance of the "test it in the shape it
			 * ships in" warning in T0.  It is harmless but it is not ours,
			 * and it hides the game behind it.  `catch` because the command
			 * only exists on the platforms that have a console.
			 */
			"catch {console hide}\n"
			"wm title . \"ZangbandTK\"\n"
			"wm geometry . 640x400\n"
			"pack [label .placeholder"
			" -text \"ZangbandTK/Tk\\n\\nT0: the window is up.\\n"
			"The game is running behind it with no display yet.\""
			" -justify center -padx 40 -pady 40]\n"
			"wm protocol . WM_DELETE_WINDOW { angband_quit }\n"
			"raise .\n"
			"focus -force .\n") != TCL_OK) {
		plog_fmt("Tcl/Tk: could not build the window: %s",
				Tcl_GetStringResult(interp));
		return 1;
	}

	term_data_link(&td_main);

	return 0;
}

#endif /* USE_TCL */
