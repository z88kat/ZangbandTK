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
 * A term and the canvas it draws into.
 *
 * The grid is one canvas text item per cell, created once and reconfigured as
 * the game writes.  That is 1,920 items for an 80x24 term, which Tk handles
 * without complaint, and it keeps T1 entirely inside core Tk -- no custom
 * canvas item, no private header.  T2 revisits rendering anyway when tiles
 * arrive, so the cheapest correct thing now is the right thing now.
 */
typedef struct term_data term_data;
struct term_data {
	term t;
	int cols;
	int rows;
	int cw;			/* cell width in pixels */
	int ch;			/* cell height */
	int ascent;
	int *item;		/* canvas item id per cell, row-major */
	int cursor_item;
	bool cursor_visible;
};

static term_data td_main;

/**
 * "#rrggbb" for each of the game's colours, rebuilt on TERM_XTRA_REACT.
 */
static char colour_name[MAX_COLORS][8];

/**
 * Cached objects for the inner loop.
 *
 * Every cell the game writes becomes one `.term itemconfigure <id> -text <s>
 * -fill <colour>`.  Going through Tcl_EvalObjv with prebuilt objects rather
 * than formatting a command string for Tcl to parse is the difference between
 * a redraw costing microseconds and costing milliseconds, and a full screen is
 * 1,920 of them.
 *
 * Only the four constant words are cached.  The three that change are made
 * fresh each call, because Tcl takes a reference to everything passed to
 * Tcl_EvalObjv: reusing and mutating them panics with "Tcl_SetStringObj called
 * with shared object" the moment the canvas has kept one.
 */
static Tcl_Obj *cfg_widget;
static Tcl_Obj *cfg_itemconfigure;
static Tcl_Obj *cfg_dash_text;
static Tcl_Obj *cfg_dash_fill;

static void colours_init(void)
{
	int i;

	for (i = 0; i < MAX_COLORS; i++) {
		strnfmt(colour_name[i], sizeof(colour_name[i]), "#%02x%02x%02x",
				angband_color_table[i][1],
				angband_color_table[i][2],
				angband_color_table[i][3]);
	}
}

/**
 * Reconfigure one cell.
 */
static void cell_set(term_data *td, int x, int y, const char *ch, int attr)
{
	Tcl_Obj *objv[7];
	int idx = y * td->cols + x;
	int i;

	if (x < 0 || y < 0 || x >= td->cols || y >= td->rows) return;

	objv[0] = cfg_widget;
	objv[1] = cfg_itemconfigure;
	objv[2] = Tcl_NewIntObj(td->item[idx]);
	objv[3] = cfg_dash_text;
	objv[4] = Tcl_NewStringObj(ch, -1);
	objv[5] = cfg_dash_fill;
	objv[6] = Tcl_NewStringObj(colour_name[attr % MAX_COLORS], -1);

	for (i = 0; i < 7; i++) Tcl_IncrRefCount(objv[i]);

	if (Tcl_EvalObjv(interp, 7, objv, TCL_EVAL_GLOBAL) != TCL_OK) {
		plog_fmt("Tcl/Tk: drawing failed: %s", Tcl_GetStringResult(interp));
	}

	for (i = 0; i < 7; i++) Tcl_DecrRefCount(objv[i]);
}

static errr Term_text_tcl(int x, int y, int n, int a, const wchar_t *s)
{
	term_data *td = (term_data *)(Term->data);
	char buf[MB_LEN_MAX + 1];
	int i;

	for (i = 0; i < n; i++) {
		int len = wctomb(buf, s[i]);

		if (len <= 0) {
			buf[0] = ' ';
			len = 1;
		}
		buf[len] = '\0';
		cell_set(td, x + i, y, buf, a);
	}

	return 0;
}

static errr Term_wipe_tcl(int x, int y, int n)
{
	term_data *td = (term_data *)(Term->data);
	int i;

	for (i = 0; i < n; i++) cell_set(td, x + i, y, " ", COLOUR_WHITE);

	return 0;
}

/**
 * Move the cursor, which is a plain canvas rectangle -- one of the four item
 * types the 2001 widget library implemented by hand and Tk has had all along.
 */
static errr Term_curs_tcl(int x, int y)
{
	term_data *td = (term_data *)(Term->data);
	char cmd[256];

	strnfmt(cmd, sizeof(cmd),
			".term coords %d %d %d %d %d ; .term itemconfigure %d -state normal",
			td->cursor_item,
			x * td->cw + 1, y * td->ch + 1,
			(x + 1) * td->cw - 1, (y + 1) * td->ch - 1,
			td->cursor_item);
	Tcl_Eval(interp, cmd);
	td->cursor_visible = true;

	return 0;
}

static void grid_clear(term_data *td)
{
	int x, y;

	for (y = 0; y < td->rows; y++)
		for (x = 0; x < td->cols; x++)
			cell_set(td, x, y, " ", COLOUR_WHITE);
}

/**
 * Point Tcl and Tk at their own script libraries.
 *
 * Tcl_Init finds init.tcl by walking up from the executable, which works for a
 * tclsh installed beside its lib directory and does not work for a game
 * executable sitting in a cmake build tree.  Rather than discover that as
 * "invalid command name" from the first script we source, say where they are.
 *
 * Inside a bundle they come from Contents/Resources/tcltk, which is the copy
 * scripts/pkg_macos_tcltk puts there; outside one they come from the build's
 * TCLTK_PREFIX.  Either way the environment wins if it is already set, so a
 * developer can point a build at a different toolchain without reconfiguring.
 */
static void set_script_library_paths(void)
{
	static char tclbuf[1024];
	static char tkbuf[1024];
	const char *resources = NULL;

#ifdef __APPLE__
	resources = macos_bundle_resources();
#endif

	if (!getenv("TCL_LIBRARY")) {
		if (resources) {
			strnfmt(tclbuf, sizeof(tclbuf), "TCL_LIBRARY=%s/tcltk/tcl9.0",
					resources);
		} else {
			strnfmt(tclbuf, sizeof(tclbuf), "TCL_LIBRARY=%s/lib/tcl9.0",
					TCLTK_PREFIX_PATH);
		}
		putenv(tclbuf);
	}

	if (!getenv("TK_LIBRARY")) {
		if (resources) {
			strnfmt(tkbuf, sizeof(tkbuf), "TK_LIBRARY=%s/tcltk/tk9.0",
					resources);
		} else {
			strnfmt(tkbuf, sizeof(tkbuf), "TK_LIBRARY=%s/lib/tk9.0",
					TCLTK_PREFIX_PATH);
		}
		putenv(tkbuf);
	}
}

/**
 * Handle a request from the game to do something outside drawing.
 *
 * TERM_XTRA_EVENT is the important one: it is where the game hands control
 * back, and therefore the only place Tk gets to process anything.  Blocking
 * there when the game says it is willing to wait is what keeps the application
 * from spinning at 100% while the player thinks.
 */
static errr Term_xtra_tcl(int n, int v)
{
	term_data *td = (term_data *)(Term->data);

	switch (n) {
		case TERM_XTRA_EVENT:
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

		case TERM_XTRA_CLEAR:
			grid_clear(td);
			return 0;

		case TERM_XTRA_FRESH:
			/* Let Tk repaint, but do not block: the game is mid-turn. */
			while (Tcl_DoOneEvent(TCL_WINDOW_EVENTS | TCL_IDLE_EVENTS
					| TCL_DONT_WAIT))
				;
			return 0;

		case TERM_XTRA_REACT:
			/* The palette may have changed under us. */
			colours_init();
			return 0;

		case TERM_XTRA_DELAY:
			if (v > 0) Tcl_Sleep(v);
			return 0;

		case TERM_XTRA_NOISE:
		case TERM_XTRA_SHAPE:
			return 0;
	}

	return 1;
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
 * angband_key -- every keystroke arrives here from Tk's binding.
 *
 * Arguments are Tk's %N (keysym as a number), %s (modifier state) and %A (the
 * character it produces, empty for a bare modifier or a function key).  The
 * job is to turn that into what ui-event.h calls a keycode plus mods, which is
 * the same representation the term build uses -- so a keymap written in one
 * works in the other, which §6 decision 13 of the plan asks for.
 */
static int objcmd_key(void *dummy, Tcl_Interp *ip, Tcl_Size objc,
		Tcl_Obj *const objv[])
{
	int keysym, state;
	const char *ch;
	keycode_t k = 0;
	uint8_t mods = 0;

	(void)dummy;

	if (objc != 4) {
		Tcl_WrongNumArgs(ip, 1, objv, "keysym state char");
		return TCL_ERROR;
	}
	if (Tcl_GetIntFromObj(ip, objv[1], &keysym) != TCL_OK) return TCL_ERROR;
	if (Tcl_GetIntFromObj(ip, objv[2], &state) != TCL_OK) return TCL_ERROR;
	ch = Tcl_GetString(objv[3]);

	/* X11 modifier bits, which Tk reports on every platform. */
	if (state & 0x01) mods |= KC_MOD_SHIFT;
	if (state & 0x04) mods |= KC_MOD_CONTROL;
	if (state & 0x08) mods |= KC_MOD_ALT;

	switch (keysym) {
		case 0xFF1B: k = ESCAPE; break;
		case 0xFF0D: k = KC_ENTER; break;
		case 0xFF09: k = KC_TAB; break;
		case 0xFF08: k = KC_BACKSPACE; break;
		case 0xFF7F: k = KC_DELETE; break;
		case 0xFF51: k = ARROW_LEFT; break;
		case 0xFF52: k = ARROW_UP; break;
		case 0xFF53: k = ARROW_RIGHT; break;
		case 0xFF54: k = ARROW_DOWN; break;
		case 0xFF50: k = KC_HOME; break;
		case 0xFF57: k = KC_END; break;
		case 0xFF55: k = KC_PGUP; break;
		case 0xFF56: k = KC_PGDOWN; break;
		default:
			/*
			 * An ordinary character.  Tk has already applied shift and the
			 * keyboard layout, so %A is what the player meant to type, and
			 * reporting KC_MOD_SHIFT as well would make the game see it twice.
			 */
			if (ch[0]) {
				k = (unsigned char)ch[0];
				mods &= ~KC_MOD_SHIFT;
			} else {
				/* A bare modifier, or a key we do not map.  Ignore it. */
				return TCL_OK;
			}
			break;
	}

	Term_keypress(k, mods);

	return TCL_OK;
}

/**
 * Source one of the front end's scripts, by name, from ANGBAND_DIR_TCL.
 *
 * Everything that reads a script goes through here.  The point is that no
 * script path is ever written down anywhere else: a development build resolves
 * ANGBAND_DIR_TCL to lib/tcl in the source tree, a bundle to the copy in
 * Contents/Resources, and a release will eventually resolve it inside zipfs --
 * and none of that reaches the scripts or their callers.
 */
static bool tcl_source(const char *name)
{
	char path[1024];

	path_build(path, sizeof(path), ANGBAND_DIR_TCL, name);

	if (Tcl_EvalFile(interp, path) != TCL_OK) {
		plog_fmt("Tcl/Tk: %s: %s", path, Tcl_GetStringResult(interp));
		return false;
	}

	return true;
}

/**
 * Build the canvas and its cells, and wire one term to it.
 *
 * Returns false if the window could not be built.  The caller must not ignore
 * that: without Term_activate there is no Term at all, and the first thing the
 * game does is Term_clear, which dereferences it.  That is a segfault in
 * init_angband with nothing on screen and nothing on stderr -- Tk closes
 * stderr on a bundled application -- so a silent failure here is about the
 * least debuggable outcome available.
 */
/**
 * Read one integer out of the script's angband() array.
 */
static bool tcl_get_int(const char *name, int *out)
{
	Tcl_Obj *v = Tcl_GetVar2Ex(interp, "angband", name, TCL_GLOBAL_ONLY);

	return (v && Tcl_GetIntFromObj(interp, v, out) == TCL_OK);
}

static bool term_data_link(term_data *td)
{
	term *t = &td->t;
	Tcl_Obj *cmd;
	int x, y;

	td->cols = 80;
	td->rows = 24;

	/*
	 * The script owns the font and measures a cell from it; we tell it how
	 * many cells we want and read the pixel size back.  Creating the font
	 * here as well would be the obvious mistake -- Tk's `font create` fails
	 * with "named font already exists", which fails the whole script.
	 */
	cmd = Tcl_ObjPrintf("array set angband {cols %d rows %d}",
			td->cols, td->rows);
	Tcl_IncrRefCount(cmd);
	Tcl_EvalObjEx(interp, cmd, TCL_EVAL_GLOBAL);
	Tcl_DecrRefCount(cmd);

	if (!tcl_source("main.tcl")) return false;

	if (!tcl_get_int("cellw", &td->cw) || !tcl_get_int("cellh", &td->ch)
			|| td->cw <= 0 || td->ch <= 0) {
		plog("Tcl/Tk: main.tcl did not report a usable cell size.");
		return false;
	}

	/* One text item per cell, created once. */
	td->item = mem_zalloc(td->cols * td->rows * sizeof(int));
	for (y = 0; y < td->rows; y++) {
		for (x = 0; x < td->cols; x++) {
			Tcl_Obj *mk = Tcl_ObjPrintf(
					".term create text %d %d -anchor nw -font termfont"
					" -fill white -text { }",
					x * td->cw, y * td->ch);

			Tcl_IncrRefCount(mk);
			if (Tcl_EvalObjEx(interp, mk, TCL_EVAL_GLOBAL) == TCL_OK) {
				Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(interp),
						&td->item[y * td->cols + x]);
			}
			Tcl_DecrRefCount(mk);
		}
	}

	/* The cursor: a rectangle, hidden until the game places it. */
	Tcl_Eval(interp,
			".term create rectangle 0 0 0 0 -outline yellow -width 2"
			" -state hidden");
	Tcl_GetIntFromObj(NULL, Tcl_GetObjResult(interp), &td->cursor_item);

	/* The four words of the inner loop that never change. */
	cfg_widget = Tcl_NewStringObj(".term", -1);
	cfg_itemconfigure = Tcl_NewStringObj("itemconfigure", -1);
	cfg_dash_text = Tcl_NewStringObj("-text", -1);
	cfg_dash_fill = Tcl_NewStringObj("-fill", -1);
	Tcl_IncrRefCount(cfg_widget);
	Tcl_IncrRefCount(cfg_itemconfigure);
	Tcl_IncrRefCount(cfg_dash_text);
	Tcl_IncrRefCount(cfg_dash_fill);

	colours_init();

	term_init(t, td->cols, td->rows, 256);

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

	return true;
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

	/*
	 * Declare the Tk we are already linked against, before initialising it, so
	 * that `package require Tk` is satisfied by this copy and never goes
	 * looking for one to dlopen.
	 *
	 * It is worth doing because Tcl's auto_path keeps the prefix the toolchain
	 * was *built* in: it is compiled into libtcl as TCL_PACKAGE_PATH and no
	 * environment variable removes it.  A bundled application can therefore
	 * still see the developer's tcltk/local/lib and, if anything asked for the
	 * package, would load a second Tk from there -- working on the machine that
	 * built it and nowhere else.  This closes that off rather than relying on
	 * nothing ever asking.
	 */
	Tcl_StaticLibrary(interp, "Tk", Tk_Init, Tk_SafeInit);

	/*
	 * Tell Tcl what our startup script is, before Tk starts.  This is not
	 * bookkeeping: TkpInit on macOS decides whether to open a Tcl console
	 * window by asking whether stdin is "nullish" and no startup script is
	 * set (tkMacOSXInit.c, around the Tk_CreateConsoleWindow call).  A
	 * bundled application launched from the Finder satisfies both, so Tk
	 * builds a console -- and building it inside Tk_Init is where the
	 * application hangs, in TkpInit's own event loop, before it has ever
	 * returned to us.  The symptom is an application that starts, shows
	 * nothing, and never exits.
	 *
	 * Naming the script we are about to source is both true and sufficient:
	 * nothing runs it on our behalf, because we are not Tcl_Main.
	 */
	{
		char path[1024];
		Tcl_Obj *startup;

		path_build(path, sizeof(path), ANGBAND_DIR_TCL, "main.tcl");
		startup = Tcl_NewStringObj(path, -1);
		Tcl_IncrRefCount(startup);
		Tcl_SetStartupScript(startup, NULL);
		Tcl_DecrRefCount(startup);
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
	Tcl_CreateObjCommand2(interp, "angband_key", objcmd_key, NULL, NULL);

	/*
	 * The window itself, its bindings and its font are lib/tcl/main.tcl's --
	 * term_data_link measures the font, tells the script how big the grid is,
	 * and sources it.
	 */
	if (!term_data_link(&td_main)) {
		/*
		 * Say so where it can be seen.  stderr is gone by now -- Tk redirects
		 * it to /dev/null for a bundled application, which is why the first
		 * version of this failure produced a crash report and no message
		 * anywhere -- but Tk itself is up, so it can show a dialog.
		 */
		Tcl_Eval(interp,
				"catch {tk_messageBox -icon error -title \"ZangbandTK\""
				" -message \"The game window could not be built.\""
				" -detail $errorInfo}");
		return 1;
	}

	/*
	 * Put the window on the screen now, before returning.
	 *
	 * Tk creates a toplevel but does not map it until the event loop runs, and
	 * ours does not run until the game asks for input -- which is after
	 * init_angband() has loaded every gamedata file.  Without this the
	 * application launches, shows nothing at all for a second or two, and then
	 * produces a window behind whatever the player was looking at.  From the
	 * Finder that is indistinguishable from a launch that failed.
	 */
	Tcl_Eval(interp, "update\nraise .\nfocus -force .\n");

	return 0;
}

#endif /* USE_TCL */
