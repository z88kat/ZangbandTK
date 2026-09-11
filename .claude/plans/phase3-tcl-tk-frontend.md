# Phase 3 — The Tcl/Tk Front End

**Status:** T0 in progress · **Phase:** 3 · **Inputs:** [decisions.md](decisions.md)
(DEC-12, DEC-13, DEC-14, DEC-21, DEC-22),
[phase2-development-plan.md](phase2-development-plan.md) §5,
[phase3-observations.md](phase3-observations.md), and the archive survey in §1 below.

The terminal build does not go away, and neither does the Cocoa one. ZangbandTK/Tk is an
**additional front end** — `main-tcl.c` alongside `main-gcu.c` and `main-sdl2.c` — that adds
menus, panels and tiles over the same game core, and ships as a second macOS application
rather than a replacement for the one people play now (§6 decision 14). Every gameplay
decision in Phase 1 and 2 stands unchanged.

---

## 0. Is a build possible?

**Yes — in stages, and the first stage is genuinely close. What is *not* possible is
unzipping the 2001 sources and typing `make`.** Three of the four things the original build
needed are gone: its build tool (Jam), its Tcl (8.3.3) and its game (Zangband 2.4.0). The
fourth — the front end itself — survives complete, and measurably ports.

Everything in this table was measured on this Mac today, not inferred:

| Question | Answer | Evidence |
|---|---|---|
| Does Tcl 9.0.4 build here? | **Yes** — `libtcl9.0.dylib`, `tclsh9.0`, `arm64`, zipfs present | [scripts/build-tcltk](../../scripts/build-tcltk), from [tcltk/](../../tcltk/) |
| Does Tk 9.0.4 build here, without X11? | **Yes** — `libtcl9tk9.0.dylib`, `wish9.0`, Aqua (Cocoa/Carbon/QuartzCore, no X11), `arm64` | the same script, `--enable-aqua` |
| Does the original front-end C survive complete? | **Yes** — 49,285 lines of bridge + 16,514 lines of widget library + 121 Tcl scripts, plus every image asset | §1 |
| Can we ship the original sounds? | **No** — 208 wav files survive, but they are personal-use-only material. 4.2's own pack replaces them | §1.4 |
| Does that C compile against Tk 9? | **Not as-is — but it converges fast.** 759 errors → **253** after a three-rule mechanical sweep, and **203 of those 253 sit in two files** | §2.2 |
| Is there a licence problem? | **No.** All 67 front-end files carry the Angband licence, word for word identical to [LICENSE.md](../../LICENSE.md) | §1.6 |
| Can the old bridge talk to Angband 4.2? | **No.** 774 `p_ptr->` references against a data model that no longer exists | §3.2 |
| Do we have to guess how the UI behaved? | **No.** `interface.html` is a written interaction spec — ten mouse rules plus per-window behaviour — and the original now runs | §1.5, §8 |

So the honest shape of the answer:

- **A window that plays the game** — Tk toplevel, tiles, keyboard, message line — is a
  small, self-contained piece of work (T1–T2). This is the milestone worth aiming at first,
  and it is worth having *for its own sake*: it is what makes Phase 2's wilderness
  observable, which is [phase2 §5](phase2-development-plan.md#5-a-note-on-phase-3-timing)'s
  argument.
- **The accessible UI in the screenshots** — the menus, the Micro Map, the Misc panel, the
  Recall and Choice windows — is the large piece of work, because it is not term output. It
  is 190 C accessor commands reading game state directly, and that surface has to be
  rewritten against 4.2 (T3–T8).

---

## 1. What survives, and where

### 1.1 The four pieces

The complete original stack survives across four locations, and each is the *only* copy:

| Piece | Location | Size | Verdict |
|---|---|---|---|
| Widget library (game-agnostic) | [archive/Tk/CommonTk-1.4/…/src/common-dll/](../../archive/Tk/CommonTk-1.4/OmnibandTk/src/common-dll/) | 16,514 lines, 11 files | **Port.** The most valuable part. |
| C↔Tcl bridge (game-coupled) | [archive/Tk/CommonTk-1.4/…/src/common/](../../archive/Tk/CommonTk-1.4/OmnibandTk/src/common/) | 49,285 lines, 32 files | **Rewrite the coupled half; port the rest.** |
| Tcl user interface | same tree, [tk/](../../archive/Tk/CommonTk-1.4/OmnibandTk/tk/) | 121 scripts, ~108,000 lines | **Adapt.** Reuse is proportional to how many old command names survive. |
| Game-side C | [archive/Tk/ZAngbandTk-240r5/](../../archive/Tk/ZAngbandTk-240r5/) | 75 files, Zangband 2.4.0 | **Reference only.** Superseded by our own 4.2 tree. |

Plus assets: `archive/Tk/image/` (Adam Bolt 16px, feature and town overlays, David Gervais
32px, isometric variants, UI button art), audited in §1.3; and `archive/Tk/sounds1`–`sounds4`
(**208** `.wav` files), which §1.4 explains we cannot use.

> **Correction to the archive note in [decisions.md](decisions.md).** That note says the Tcl
> UI "survives **solely** in `archive/zangbandtk/OmnibandTk-1.4/tk/`" — a stripped Windows
> runtime. It does not. `commontk-1.4-src.zip` carries **all 121** scripts, and the runtime
> copy carries 116. All 66 shared top-level scripts are **byte-identical**; the source copy
> additionally has `debug.tcl`, `debug-alloc.tcl` and `errorInfo.tcl`.
>
> **Take [archive/Tk/CommonTk-1.4/OmnibandTk/tk/](../../archive/Tk/CommonTk-1.4/OmnibandTk/tk/)
> as canonical.** It is a strict superset from the source distribution, and it keeps the
> scripts next to the C that serves them.

### 1.2 What the Tcl actually is

Not a thin skin. Broken down:

| Part | Lines | What it is |
|---|---:|---|
| `tk/*.tcl` (69 files) | 86,265 | The application: main window, menus, birth, stores, buildings, knowledge, inventory, pets, options, keymaps |
| `tk/library/*.tcl` (26 files) | 10,636 | A general-purpose megawidget set — tabbed frames, toolbars, balloons, list canvases, progress windows |
| `tk/vault/*.tcl` | 6,065 | The tile-assignment editor |
| `tk/msgs/` | 34 catalogues | Message localisation, already externalised |

The `library/` set is the one piece with a modern alternative: Tk 9 ships **Ttk**, which
covers tabbed frames, toolbars, progress bars and notebooks natively. Replacing
`library/` with Ttk trades ~10,600 lines of 1999-era megawidget code for platform-native
appearance — and native appearance on macOS is most of what "accessible" means now.

### 1.3 The graphics — audited asset by asset

**The original artwork is complete.** Audited by extracting every `.gif`/`.ico` reference
from all 121 scripts and all 16 tile configuration files, then searching the whole archive
for each. Every image the shipped UI actually loads is present. The only apparent misses
are shell-variable interpolations (`$charName.gif`, `preview-$prefix.gif`), dead code
(`adam16_shape.gif`, inside `if 0 {…}`), and one asset behind both a dead branch *and* a
`file exists` guard (`darken.gif`). Nothing real is absent.

What that comes to, by kind:

| Asset | Files | Size | 4.2 equivalent? |
|---|---:|---|---|
| Adam Bolt map tiles | `adam16.gif` | 512×560 (32×35 tiles) | **Yes, and better** — see below |
| David Gervais 32px | `dg/dg_*.gif` | 42 sheets, incl. `dg_iso32.gif` (756×735) | Yes, `lib/tiles/gervais/` |
| "Original" 32px masked set | 13 files (`masked`, `potion`, `jelly`, `vortex`, `mushroom`, `dragon`, …) | 32px | Roughly, `lib/tiles/old/` |
| **Wall/terrain overlay** | `feature16/32.gif`, `adam16_feature.gif` | 256×64, 512×128, 160×160 | **No** |
| **Town and wilderness tiles** | `town16/24/32.gif` | 176×128 … 352×256 | **No** |
| **The Pattern** | `pattern16.gif` | 160×32, 10 tiles | **No** |
| **Micro Map tiles** | `dg/micromap4x4/6x6/8x8.gif` | 24×20 … 48×40 | **No** |
| **Building service icons** | `dg/townactions.gif` | 256×288, 72 icons at 32px | **No** |
| **UI chrome** | 39 files — toolbar buttons, nav arrows, checkboxes, the 13 Misc-panel stat icons | 16–20px | **No** |
| Tileset previews | `preview-*.gif` | 13 | n/a — for the options dialog |

The rows marked **No** are the answer to "do we have the original graphics". The map tiles
are the *least* valuable thing in the archive, because 4.2 maintains its own copies. What
4.2 has none of is everything that made ZangbandTK look like ZangbandTK rather than
Angband-with-tiles: the shaded wall overlay, the town buildings, the Micro Map, the
building-service icons, the Misc panel, and the toolbar.

**`pattern16.gif` deserves singling out.** Ten 16px tiles for Zangband's Pattern — which is
the Pattern *of Amber*. It is the one piece of original art that is directly on DEC-01's
target, and it exists nowhere else.

> **A bonus:** `dg/misc-win/stats.gif` (190×193) is not a tile sheet — it is a **rendered
> mock-up of the finished Misc window**, stat icons in place. Less critical now that the
> original runs (§8), but still the clearest single statement of how that panel was meant to
> be read.

#### The catch: the tile *art* survives, the tile *indices* do not transfer

The archive's Adam Bolt sheet and 4.2's are the same artwork, independently reorganised.
4.2's is 512×**976** against the archive's 512×**560** — 4.2 added terrain rows at the top
and inserted content throughout. Comparing the overlapping region pixel for pixel: **64% of
pixels differ**. Same art, different addresses.

That matters because the labour in `archive/Tk/ZAngbandTk-240r5/ZAngbandTk/tk/config/` is
**8,344 lines of assignment data** — `adam-assign` (1,493 lines), `dg32-assign` (1,561),
`classic-assign` (430), plus `*-alternate` (per-terrain variants), `*-sprite` (animation)
and the `.vlt` town vault layouts. Every line is keyed to *Zangband 2.4.0's* feature,
monster and object indices. It is marked `# Automatically generated. Do not edit.` because
the in-game vault editor produced it.

So: **read it as intent, do not load it as data.** Phase-2 milestones are adding and
renumbering content anyway, so no index-keyed table from 2001 could survive. For the map
tiles this costs nothing — 4.2's `graf-*.prf` files already do the job. It only bites for
the sets 4.2 has no assignment path for at all (town, feature overlay, pattern), and there
the old files say *which tile the author chose* for each concept, which is the part worth
knowing.

### 1.4 The sounds — present, and mostly unusable

Two packs survive, and they are in completely different legal positions.

| Pack | Files | Format | Ships? |
|---|---:|---|---|
| Default, in `CommonTk-1.4/OmnibandTk/lib/sound/` | 27 wav, 404 KB | 25 of 27 are 8-bit; 23 at ≤11 kHz | **Doubtful** |
| Grundman's Ultimate Sound Collection, `archive/Tk/sounds1`–`sounds4` | 208 wav, 17.5 MB | 69% 8-bit; 56% at ≤11 kHz; only 3 files at 44.1 kHz | **No** |

**The 208-file collection cannot be redistributed.** Its own `readme.txt`, from the author:

> Important Note! Do not distribute any of these files commercially as many of them are
> copywrited. These files are for your own personal use ONLY.

And the download page is firmer still — "may not be redistributed or used commercially in
any way … for your personal use only". It was an optional separate download in 2001 for
exactly this reason. Filenames like `godzilla.wav` and `onlyone.wav` say the rest.

The default 27-file pack shipped bundled, so it was at least distributed once, but it
carries no licence statement of its own and its filenames — `doh`, `doheth`, `duh`,
`woohoo`, `yorica` — do not suggest cleared material either. And the fidelity is not worth
fighting for in either pack: **69%** of the Grundman files are 8-bit and **56%** run at
11 kHz or below, against 44.1 kHz for what 4.2 already ships.

**So the audio is a dead end, and it does not matter, because 4.2 already solved this.**
[lib/sounds/](../../lib/sounds/) holds the Dubtrain Angband Sound pack: 213 mp3s, 44 kHz,
3.2 MB, under a Creative Commons **non-commercial** licence — which is exactly the footing
ZangbandTK itself ships on. Use it.

#### What *is* worth taking: the event vocabulary

The valuable artefact here is not audio, it is `archive/Tk/grund/grund.snd` — **919 event
bindings** across five groups (`event`, `monster`, `monster_spell`, `monster_attack`,
`martial_art`). That is design data, not copyrighted material, and it is a far richer
vocabulary than 4.2's:

| | ZangbandTK (`grund.snd`) | Angband 4.2 (`sound.prf`) |
|---|---:|---:|
| Total bindings | 919 | 149 |
| Distinct event names | 211 symbolic + 672 per-monster | 149 |
| Per-monster sounds | **672 monsters** | none |
| Monster spells / attacks / martial arts | separate groups | folded into the flat list |
| Ambient day / night / dungeon-depth beds | none | **8** |

The two lists barely overlap by name — 7 exact matches — but that understates it badly. Most
of the difference is Zangband's four-character truncation against 4.2's full spelling, and
those map by eye: `BR_CHAO`→`BR_CHAOS`, `BR_COLD`→`BR_FROST`, `BR_DISE`→`BR_DISEN`,
`BR_INER`→`BR_INERTIA`, `BR_LITE`→`BR_LIGHT`, `BR_NETH`→`BR_NETHER`, `BR_GRAV`→`BR_GRAVITY`,
`BR_SOUN`, `BR_SHAR`, `BR_PLAS`, `BR_NEXU` — 11 more confirmed in the breath family alone.
So the 211 symbolic names are a hand-mappable table, not a redesign.

The 672 per-monster bindings are the same problem as the tile indices in §1.3: keyed to
Zangband 2.4.0's `r_info` numbering, so read as intent, not loaded as data. And two
referenced files are genuinely absent (`godzilla.wav`, `onlyone.wav`, between them bound to
four monster events) — which is moot, since none of the pack ships.

> **Consequence for the design.** 4.2's sound system is message-driven and flat; the old one
> had a per-monster tier and separate spell/attack groups the Tcl **Sound Window** let the
> player edit live. If that window is worth reviving in T9, `sound-core.c` needs a
> per-monster tier that 4.2 does not currently have. That is a real feature decision, not a
> port detail — flagged in §6.

### 1.5 The documentation — the interaction spec, already written

The single most useful thing in the archive is not code. It is
[`tk/doc/`](../../archive/Tk/CommonTk-1.4/OmnibandTk/tk/doc/) plus the variant's own doc
directory, and it means **the interaction model does not have to be reverse-engineered from
screenshots**.

| File | Size | What it is |
|---|---:|---|
| `interface.html` | 11 KB | **The interaction specification.** Window-by-window behaviour, then ten numbered mouse rules. |
| `tips.txt` | 2.3 KB | **32 one-line tips**, each naming one affordance. The startup Tips window cycles them. |
| `TANG.html` | 36 KB | *The Angband Newbie Guide*, shipped in-game |
| `variant-faq.txt` | 31 KB | Angband variant FAQ |
| `changes.html` | 76 KB | Release notes back to ZAngbandTk 2.2.5r1 |
| `options.txt` | 29 KB | Option reference |
| `contents`, `contents-zkb`, `help-index` | 14 KB | The help tree (see below) |
| `performance.html`, `files.html`, `about.html`, `original.html` | 6 KB | Author's notes, including his own performance advice |
| `msgs/` | 34 catalogues | **English *and* Japanese**, `en.msg` + `ja.msg` per area. Not carried forward — §6 decision 11 |

#### What `interface.html` pins down

Ten numbered mouse rules for the Main Window, which between them are the whole
mouse-driven game:

| Gesture | Effect |
|---|---|
| Left-click | Step one grid toward the click; hold to walk continuously |
| Shift-left-click, or right-click | **Run** in that direction |
| Control-left-click | `Alter (+)` — tunnel, open, disarm |
| Hover | `Look` — describes the grid; over a monster, shows the health bar and recall |
| Right-click *while prompted for an item* | Popup menu of inventory choices |
| Control-right-click | Cascading menu of items to use, unprompted |
| Click *while prompted for a direction* | Aim that way; right-click targets what is under the pointer |
| Left-click *while targeting* | Target the monster there, else the location |
| Click during `-more-` | Skip the prompt (with `quick_messages`) |
| Click during a repeated command | Interrupt the run, rest or tunnel |

And per window: the Micro Map's popup resolution menu, click-drag scroll, and Shift-click to
pin the Main Window; the Recall Window growing on pointer-enter and shrinking on leave;
Choice and Recall swapping roles depending on which is open; the Inventory's per-item
right-click command menu; the Misc Window's clickable `EXP` and `AC` labels that toggle
between total and remaining, and total and base+bonus; double-click to buy in a store.

> **A worked example of why this matters.** Hovering the Micro Map moves a **yellow
> rectangle** in the Main Window to the corresponding grid — a reciprocal linked cursor,
> confirmed live. It is a `cursor` canvas item (`-color yellow -linewidth 2`), created at
> `main-window.tcl:1503` for the Main Window and `misc-window.tcl:426` for the Micro Map.
>
> That item type is implemented in **`widget2-dll.c`** — the file carrying 155 of the 253
> remaining compile errors. So the §7 fallback of "drop the custom canvas items for a plain
> Tk canvas" is not free: it would have to reimplement `cursor` along with `progressbar`,
> `text` and `rectangle`. Worth knowing before that fallback is chosen, not after.

Treat `interface.html` and `tips.txt` as **T5 and T7 acceptance criteria**. Each rule is a
testable statement, and between them they define what "the menus work" actually means.

#### The help system is TkHtml, not WinHelp

It looks like HTML Help 1.x — tree pane, Hide / Back / Next / Refresh toolbar — and on
Windows that was the point. It is not: `help-html.tcl:40` does `package require Tkhtml`, so
the whole thing is a **Tk-native help browser** rendering the same HTML files, with the
Windows chrome imitated. That is why it still runs on macOS, where a real `.hlp` would not.

The tree comes from `tk/doc/contents`, a small declarative format worth keeping:

```
.book "The Interface" interface.html
.page "Using The Interface" interface.html#using-the-interface
.page "The Mouse"          interface.html#the-mouse
.stop
```

`help-html.tcl` then *generates* the Table of Contents page from that tree at runtime.

**But TkHtml has to go.** It is a binary Tcl extension by D. Richard Hipp, 1997–98, against
Tk 8.0-era internals, and the archive ships only its `pkgIndex.tcl` and `COPYRIGHT` — the
compiled library came from `tkhtml.zip`. It will not build against Tk 9, its successor
Tkhtml3 is also long unmaintained, and it is **LGPL v2**, a different licence from
everything else here. Three independent reasons, any one sufficient.

DEC-17 already commits ZangbandTK to a Sphinx manual in [docs/](../../docs/). The help
system should render *that*, and the `contents` format is a good model for its index. New
decision in §6.

### 1.6 Licensing — checked, and clear

All 67 front-end source files carry:

> Copyright (c) 1997-2001 Tim Baker. This software may be copied and distributed for
> educational, research, and not for profit purposes provided that this copyright and
> statement are included in all such copies.

That is the **Angband licence**, word for word — the licence ZangbandTK already ships under
per [LICENSE.md](../../LICENSE.md). Nothing to resolve. Keep the copyright headers on
anything ported, exactly as DEC-20 already requires for Zangband code.

Two dependencies are *not* clear and must stay out: **BASS** (music, non-commercial
licence, Windows-only) and **Hermes**. Both are already `#ifdef`-optional in the original
and neither is needed.

---

## 2. Verified build facts

### 2.1 Tcl/Tk 9.0.4 on Apple Silicon — built, and now reproducible on demand

DEC-13 holds, and **T0's first deliverable is done**:
[scripts/build-tcltk](../../scripts/build-tcltk) builds both from the sources vendored in
[tcltk/](../../tcltk/) into `tcltk/local`, in about two minutes on this Mac. Result, 11 Sep:

| | |
|---|---|
| `libtcl9.0.dylib`, `libtcl9tk9.0.dylib` | `arm64`, both |
| `tclsh9.0`, `wish9.0` | run; `zipfs root` answers `//zipfs:/` |
| Windowing system | **Aqua** — `wish9.0` links Cocoa, Carbon and QuartzCore, and no X11 at all |
| Private headers | `tclInt.h` and `tkInt.h` installed, which §2.2's port requires |
| Prefix | project-local, gitignored; no system Tcl is touched or needed |

**Both bootstrap traps are avoided by construction rather than worked around**, which is
worth recording because the workaround was the plan:

1. **The one DEC-13 records.** Tcl's `configure-packages` step runs the freshly built
   `tclsh`, which is linked against its *install* path. On a clean tree that dylib does not
   exist yet, so the step dies with "cannot find a usable native Tcl 9 tclsh" although the
   core compiled fine.

2. **A second one, found while writing this.** The bundled `thread3.0.6` package fails its
   zip step (`cp: libthread.vfs/thread_library/ttrace.tcl: No such file or directory`) on
   an out-of-tree build. The Tcl **core** is unaffected — only the bundled extras break.

Both live in the `packages` step, and ZangbandTK needs none of the four bundled packages
(thread, sqlite, itcl, tdbc). Building `binaries libraries` rather than the default `all`
skips that step entirely, so neither trap fires. Do **not** reach for `--disable-zipfs`:
zipfs is worth keeping, because it is how the front end's Tcl gets packaged inside the
executable instead of shipped as a loose directory.

The third trap is new and cost nothing only because the script now encodes it: **`make
install` does not install `tkInt.h`.** The port's riskiest files reach into it (§2.2), and
without `install-private-headers` for both Tcl and Tk the front end compiles against the
source tree here and nowhere else — a failure that would first appear in CI, on a machine
where nobody could see the source tree at all.

### 2.2 The port measured, not guessed

Syntax-checking all 11 game-agnostic widget-library files against Tk 9.0.4 headers:

| State | Errors |
|---|---:|
| Untouched 2001 source | **759** |
| After a three-rule mechanical sweep | **253** |

The sweep is three substitutions: `CONST`/`CONST84` → `const` (Tcl 9 removed the macro),
`ClientData` → `void *`, and `int objc` → `Tcl_Size objc` in command procedures. That one
change clears **two thirds** of the errors, because a single unparseable
`Tcl_Obj *CONST objv[]` parameter cascades into every use of `objv` in the function body —
238 of the original 759 errors were that cascade alone.

Where the remaining 253 sit is the useful part:

| File | Errors | Character |
|---|---:|---|
| `widget2-dll.c` | 155 | Custom canvas items — `TextItem`, `ProgressItem`, `RectItem`, `CursorItem` |
| `widget1-dll.c` | 48 | Custom widget core — `Widget`, `TkClassProcs` |
| `widget-dll.h` | 15 | its header |
| everything else (8 files) | **35** | icon engine, map, event binding, struct accessors, platform, cmdinfo |

**This confirms DEC-14's prediction precisely.** The concentrated risk is the custom widget
implementations reaching into `tkInt.h`, and the tile/icon engine — the part actually worth
having — is nearly clean. 35 errors across eight files is a day's work, not a rewrite.

> **A finding that amends DEC-12.** DEC-12 says platform shims are "small" and "Tk handles
> drawing portability itself". Half right. `plat-dll.c` *is* small — 370 lines behind a
> six-function API — but its fast blit path has exactly two implementations: a Windows DIB
> section, and an X11 **MIT-SHM** shared pixmap. Aqua has neither, and `PLATFORM_MAC`
> appears in the sources only inside `#error` guards — there is no macOS code behind it.
>
> DEC-12's list of what `plat.c` abstracts is also one short: besides the font chooser,
> X-window-to-HWND conversion, `system` and the millisecond timer, there is **the desktop
> work area** (`angband system workarea`), which every window's geometry depends on
> ([OBS-37](phase3-observations.md)). On macOS that is the screen's visible frame minus menu
> bar and Dock.
>
> So macOS needs a **third** `plat` backend. The good news is that the fallback already
> exists in the same tree: `icon-dll.c` uses `Tk_PhotoPutBlock` for the portable path.
> Start there, measure, and only write a Core Graphics fast path if the portable one is too
> slow. Keep this behind `plat`, per DEC-12 rule 2.

---

## 3. Architecture

*Rewritten 11 Sep 2026. The previous version modelled the bridge as 190 accessor commands
with Tcl reading game state and replying in keystrokes — the original's model, which it was
forced into because 2.4.0 offered nothing else. 4.2 offers three more seams, and two of them
retire whole command families outright. Everything below is checked against the tree rather
than inferred from the archive.*

### 3.1 What the original actually looked like

From the screenshots at
[old-games.com/download/4122/zangbandtk](https://www.old-games.com/download/4122/zangbandtk),
ZAngbandTk 2.4.0r5 was **six windows**, not one:

- **Main** — menu bar (File, Inven, Book, Action, Other, Tool, Window, Help) over a scrolling
  16px tile map, with hover name tooltips and a health bar under the hovered monster
- **Micro Map** — a whole-level overview
- **Misc** — an icon toolbar plus a compact character panel (level, race, class, gold, AC…)
- **Progress** — HP/SP/Food as labelled bars
- **Recall** — the monster/object recall text
- **Choice** — the current item list, as a selectable menu
- **Messages** — scrolling message log

The menu bar is the whole point. `Action`, `Book`, `Inven` are how you avoid memorising
`^`, `m`, `b`, `E`, `q`, `r`, `u`, `z`, `a`. That is what "more appealing to novices" meant
in 2000, and it is what the user is asking for now.

One screenshot is worth noting: the Character Info window is **plain terminal text in a Tk
toplevel**. The original mixed native widgets and term windows freely, and shipped
term-rendered screens wherever a native widget was not worth building. That is licence to
do the same, and it is what makes T1 useful on its own.

### 3.2 The four seams

The original bridge had **one** seam and everything followed from it: Tcl read game state
through accessor commands, and pushed keystrokes back. That is why there are 190 commands,
why `angband keypress` is the second most-used family in the whole script tree, and why
`inkey_flags` exists at all — with only keystrokes to reply with, Tcl had to *infer* what
the game was currently asking before it could answer.

4.2 has four seams. Three of them did not exist in 2001, and two of those retire a command
family outright:

| Direction | 4.2's seam | Retires | Verified at |
|---|---|---|---|
| **UI drives the game** | the typed command queue — `cmdq_push()` plus `cmd_set_arg_item / _direction / _point / _number / _string / _choice / _target` | `angband keypress` synthesis | [cmd-core.h:317-383](../../src/cmd-core.h#L317) |
| **Game says something changed** | `event_add_handler()` / `event_add_handler_set()` over **66** event types | `qebind`'s game-coupled half | [game-event.h:29](../../src/game-event.h#L29), [:211](../../src/game-event.h#L211) |
| **Game asks the UI a question** | **17 input hooks** — `get_item_hook`, `get_spell_from_book_hook`, `get_point_hook`, `get_quantity_hook`, `get_check_hook`, `view_abilities_hook` and the rest | `inkey_flags` inference | [game-input.h](../../src/game-input.h), installed at [ui-input.c:1788](../../src/ui-input.c#L1788) |
| **UI reads state to draw** | nothing — this is the bridge we write | — | — |

**What this changes, concretely.** `keypress` (32 scripts) and `inkey_flags` (18) are the
two largest families after `player`, and between them they are the *whole* of the old
input model. Both are replaced by API that already exists and is already typed: the front
end pushes a `CMD_DROP` carrying an object pointer rather than synthesising `d` and then
guessing which prompt arrived. A hook is *called with* its context and *returns* an answer,
so there is nothing to infer.

**What this does not change.** The fourth row is empty on 4.2's side, and it is the bulk.
Drawing the Misc panel means reading `player`; the Micro Map means reading `cave`; Recall
means reading `monster_race`, `object_kind` and `artifact`; the paper doll means reading
`player->body`. No hook and no event hands you that — every one of those is an accessor we
write. So the honest revision is not "the bridge collapses":

- The **driving and prompting third** of the old surface is replaced by existing API.
- The **reading two-thirds** is unchanged in size, and is still keyed to structures that
  have been renamed and reshaped since 2.4.0 — the 774 `p_ptr->` references are all in
  this part.

§4's ~24,800-line rewrite estimate should be re-scoped on that basis: `interp1.c` and
`interp2.c` are where the saving falls, and it is a saving in the third of the bridge that
was going to be the fiddliest, not in the two-thirds that is merely long.

> **The trap in the third row, and it is the important one.** The hooks belong to the
> *text UI*, not to the front end. [`textui_init()`](../../src/ui-init.c#L44) calls
> `textui_input_init()`, which installs all seventeen, and `main.c` calls it at
> [line 565](../../src/main.c#L565) — before `play_game()`. So `main-tcl.c` links the whole
> text UI and overrides hooks *after* that call, one at a time.
>
> That is exactly the property that makes T1 → T7 incremental: **each native dialog is one
> hook reassignment**, and everything not yet converted keeps working because the term
> implementation is still underneath it. It is also what makes the milestone boundaries
> real rather than administrative.
>
> The caution: **nothing in the tree overrides an input hook today.** `main-gcu.c`,
> `main-sdl2.c` and `main-win.c` all set *term* hooks and take textui's input hooks as they
> come. The seam is real, it is declared, and it is unexercised — T3 is its first consumer,
> so budget for finding its rough edges rather than assuming a clean fit.

**The menu bar falls out of the same tree.** [`struct cmd_info`](../../src/ui-input.h#L44)
carries `desc`, `key[2]`, a `cmd_code`, a `hook` and a `prereq` predicate, and
[`cmds_all[]`](../../src/ui-game.c#L386) groups those into named lists. Label, accelerator,
action and enable-state for every command in the game, already declarative. Commands with a
real `cmd_code` go to the queue; commands with `CMD_NULL` and a `hook` are UI actions the
front end calls directly. T7 needs no leaf data of its own.

### 3.3 Three tiers, and what feeds each

DEC-14 settles the model: `main-tcl.c` is a term backend, the game keeps its main loop.
That gets you part of the UI. **Three tiers, not two** — the middle one is invisible from
screenshots and is where the original did its best work ([OBS-15](phase3-observations.md)):

| Tier | Fed by | Gets you |
|---|---|---|
| **Term windows** | 4.2's existing `ui-term.c` hooks — `Term_text`, `Term_pict`, `Term_curs`, `Term_xtra` | The map and messages, and any full-screen display we choose not to rebuild |
| **Canvas text laid out as a terminal** | read accessors, drawn as canvas items | Character Info (1,170 lines), the flags table (673), the Misc panel. *Looks* classic; is addressable and clickable — which is how the Misc window's `EXP` and `AC` labels toggle on click |
| **Native widgets** | all four seams — accessors to draw, events to know when to redraw, hooks to answer prompts, the queue to act | Micro Map, Progress, Recall, Choice — and the menus |

The middle tier matters because it dissolves a false choice. A player who reads the classic
sheet fluently loses nothing by it being a canvas, and gains hover, colour and click. Reach
for it wherever the classic layout *is* the right layout — bounded by §6 decision 9, which
makes native the default and this the argued exception.

Measured demand per command family, as script count. Read it now as **two lists, not one**:

| Replaced by a 4.2 seam | Scripts | Still ours to write | Scripts |
|---|---:|---|---:|
| `keypress` → command queue | 32 | `player` | 36 |
| `inkey_flags` → input hooks | 18 | `r_info` | 11 |
| `sound` → `sound-core.c` (§4) | 9 | `k_info` / `inventory` | 10 each |
| `setting` → 4.2's option table | 7 | `o_list` / `info` | 8 each |
| `keymap` → 4.2's keymap API | 5 | `cave` | 7 |
| | | `spell` / `mindcraft` | 6 each |
| | | `store` / `m_list` / `power` / `equipment` | 5 each |
| | | `game` / `system` | 10 each |

**The rule this sets, and it applies to the right-hand column: keep the old command names
wherever 4.2 has an equivalent concept.** It is the single highest-leverage decision in
Phase 3 — it is what turns 108,000 lines of Tcl from a rewrite into an adaptation. Where
4.2's concept genuinely differs, rename deliberately and fix the scripts; never rename for
taste. The left-hand column is exempt: those scripts change no matter what they are called,
because what they *do* is being replaced.

**`qebind` splits along the same line.** §4 budgets `qebind-dll.c` in the Port column, and
that is still right for its Tcl-facing half — the machinery by which a script says "call me
when this changes" is genuinely game-agnostic and worth having. Its game-facing half, the
event *sources*, is replaced by one C shim that turns `event_add_handler` callbacks into
qebind events. Port the binder, drop the sources.

---

### 3.4 What the original UI assumed about the world — and why that matters here

Playing the original raises a fair question: the vault editor and the `.vlt` files look like
evidence of a hand-authored world. They are, and the evidence is stronger than the vault
editor. But it does not carry over, because **Zangband changed its wilderness architecture
between the version ZangbandTK was built on and the version this project derives from.**

| | ZAngbandTk 2.4.0r5 (the running reference) | Zangband 2.7.5-preview ([archive/zangband/](../../archive/zangband/), the Phase 1–2 source) |
|---|---|---|
| `w_info.txt` | 1.1 KB | 23.7 KB |
| Hand-drawn map rows (`W:D:`) | **10** — a literal 10×10 ASCII world | **0** |
| What the file defines instead | Terrain letters, five towns at fixed coordinates, roads with N/S/E/W flags, a fixed start | **232 wilderness block types**, each as bounds in a 256×256×256 parameter space |
| Parameters | none | `hgtmin/hgtmax` (height), `popmin/popmax` (population), `lawmin/lawmax` (law vs chaos), plus a generator function number |
| Towns | 5 static templates, `t0000001.txt`–`t0000005.txt`, selected by `$TOWN` | generated |
| World | **authored** | **generated** |

2.4.0's whole world is these ten lines, towns and roads included:

```
##########        1 Outpost      G Grass       M Mountain
#GGGGMVMM#        2 Telmora      D Dirt        V Volcano
#GGG3GGMM#        3 Morivant     F Forest      - road E-W-S
#GGG|GGGG#        4 Angwil       S Shallow     | road N-S
#GGG1-2GG#        5 Wilderness   W Deep water
#GGDD|DGG#
#WSFF4FFS#
#WWSSSSSS#
#WWWW5WWW#
##########
```

2.7.5 has no such map. Its `w_info.txt` is a table of block *types* selected by position in
parameter space — which is precisely the "height/population/law decision tree" the
[Phase 2 M4 table](phase2-development-plan.md) already cites.

**Settled — the wilderness stays generated.** Confirmed by project owner: *"I would not
implement a static map - we stay with our generated one."* This changes nothing in Phase 2;
M4 was already built on 2.7.5's generator. Recording it because the running reference
demonstrates the *other* design persuasively, and someone will ask again.

**Three consequences for Phase 3, which is why this sits here and not only in Phase 2:**

1. **It settles §6 decision 2 harder.** `town.vlt` and `t0000001-dg32.vlt`–`t0000004…` are
   icon assignments *per static town template*. With generated towns there are no templates
   to paint, so the vault editor loses its main use case outright rather than merely being
   redundant. Drop `tk/vault/` — 6,065 lines — without hesitation.
2. **The overhead world map (WLD-25) has a different hook.** 2.7.5's `w_info.txt` carries an
   `M:` field per block type — "the feature to mimic in `f_info.txt`" — so every block
   declares its own overhead glyph. T5's overhead map should read that, not a static image.
   This is a better hook than 2.4.0 had, and it is the one the Micro Map should build on.
3. **The Micro Map cannot assume a bounded world.** 2.4.0's was 10×10 and fully known; ours
   is generated and unbounded in principle. The resolution popup and click-drag scrolling
   from §1.5 still apply, but "fit the whole world in the window" does not. Design the
   Micro Map around the current chunk neighbourhood, not the world.

*Content worth keeping regardless:* the five town names — **Outpost, Telmora, Morivant,
Angwil** and **Wilderness** — are Zangband's own, and are cheap to preserve as generated
towns' names. Check them against DEC-01's Amber target before adopting them wholesale.

---

## 4. The four-way split of the old code

| Verdict | What | Lines | Note |
|---|---|---:|---|
| **Port** | `icon1/2.c`, `icon-dll.c` (tile engine), `map-dll.c`, `qebind-dll.c`, `struct-dll.c`, `util-dll.c`, `cmdinfo-dll.c`, `plat*.c` | ~12,000 | Game-agnostic. 35 compile errors between them. The real prize. `qebind-dll.c` is the one exception: port its binder, drop its event sources for `game-event.h` (§3.3). |
| **Port, with the risk** | `widget1/2-dll.c`, `canv-widget.c`, `widget.c`, `TclTk-dll.c` | ~8,000 | Private-header widgets. 203 of the 253 remaining errors. |
| **Rewrite against 4.2** | `interp1.c`, `interp2.c`, `struct.c`, `town.c`, `describe.c`, `r_info.c`, `birth-tnb.c`, `wor-z.c`, `file_character.c`, `main-tnb.c` | ~24,800 | The bulk of Phase 3. |
| **Drop** | see below | ~10,000 | |

**Drop, with reasons:**

- **`sound.c` (2,705 lines) and all seven sound backends.** 4.2 already has
  [sound-core.c](../../src/sound-core.c), [snd-sdl.c](../../src/snd-sdl.c) and **213**
  packaged sounds. Building a second sound system beside it would be perverse. This is
  *better* than DEC-12's "sound is already solved — seven pluggable backends exist": the
  right move is to drop all seven and expose 4.2's own to Tcl. **The 208 archived `.wav`
  files go too** — they are personal-use-only material that cannot be redistributed
  (§1.4). What survives from the old sound system is `grund.snd`'s event vocabulary.
- **Jam / JamTk / the `.jamtk` project files.** Replaced by a `TCL_Frontend.cmake` module
  matching the existing [src/cmake/macros/](../../src/cmake/macros/) pattern.
- **The `common-dll` / `main-boot.c` DLL split and the stub generator** (`shlib.c`,
  `stubs.c`, all ten `*-dll.h`). That existed so one launcher could host four *bands. We
  ship one game; link it statically.
- **`dbwin`, `TclZip`, `TkHtml`, `Img`, `UpgradeTool`, `install.exe`.** All three binary Tcl
  extensions the original needed are dead, and **Tk 9 core replaces every one**: PNG reading
  is built in (no `Img`), zipfs replaces `TclZip`, and Sphinx replaces `TkHtml`
  ([OBS-26](phase3-observations.md)). `dbwin` is a Windows debug console.
- **Python and Hermes hooks.** Already `#ifdef`-optional, already unused.
- **`notes.c`.** Zangband wrote player notes to `lib/save/<charname>.txt`; 4.2 has the same
  `:` command routing into the character history via `history_add()`
  ([OBS-18](phase3-observations.md)). Strictly better, already there.
- **`savefile-z.c` (1,240 lines).** It existed to read savefile metadata without loading a
  game, for the Open dialog's character list. 4.2 has that as public API —
  `savefile_getter`, `got_savefile()`, `get_savefile_details()` returning `{fnam, desc,
  foff}` ([ui-game.h:50](../../src/ui-game.h#L50)). Dropped, not rewritten. See
  [OBS-01](phase3-observations.md).

**Two open calls, not yet decided** — see §6.

---

## 5. Milestones

Each ends with something runnable. T1 is the one to aim at before deciding anything else.

### T0 — Build foundation
*No visible change. Everything here is cheap now and expensive to retrofit, which is the
only reason any of it is in the first milestone.*

- ✅ **`scripts/build-tcltk`** — done, 11 Sep. Builds both from `tcltk/` into `tcltk/local`
  in about two minutes; see §2.1 for what it verifies and the three traps it encodes.
- `src/cmake/macros/TCL_Frontend.cmake` + `SUPPORT_TCL_FRONTEND` option, defaulting **OFF**,
  matching the existing front-end modules, plus `TCLTK_PREFIX` pointing at what the script
  installed.
- `src/main-tcl.c` from [main-xxx.c](../../src/main-xxx.c): embeds the interpreter, opens one
  toplevel, registers nothing.
- Vendor the ported widget library under `src/tcl/`, with copyright headers intact.
- Apply the §2.2 mechanical sweep as **one reviewable commit**, separate from every
  hand-fix that follows. It touches every file and must not hide real changes.

Four more, each moved here from a later milestone because doing it later means doing it
twice — see §6 decisions 11–13:

- **`ANGBAND_DIR_TCL` and one loader.** The scripts live in `lib/tcl/`, and the path is
  built beside the other thirteen at [init.c:355](../../src/init.c#L355). Everything that
  reads a script goes through one function, so a dev build can point at the source tree and
  a release build at zipfs without a single `source` path changing. Deciding this at T9,
  when there are 121 scripts, means editing 121 scripts.
- **A minimal `ZangbandTclTK.app` bundle**, with its own identifier — a second application
  beside the Cocoa one, never a replacement for it (§6 decision 14). Unbundled Tk on Aqua
  opens a window, but the application menu, Dock identity, activation and the native file
  dialogs all behave differently from a bundled app — and T7 commits to the application menu
  and ⌘-accelerators specifically ([OBS-42](phase3-observations.md)). An `Info.plist`
  wrapper now means every milestone from T1 on is tested in the shape it ships in, rather
  than discovering at T7 that the shape was wrong all along.
- **`.github/workflows/tk.yaml`.** macOS only to begin with (§6 decision 13), building the
  toolchain and then the front end. Cache `tcltk/local` keyed on a hash of `tcltk/` so the
  two-minute toolchain build happens once rather than per push.
- **The test harness skeleton**, per §9 — `tcltest` wired up and running zero tests, so that
  the first test written has somewhere to go.

**Exit:** `cmake -DSUPPORT_TCL_FRONTEND=ON -DTCLTK_PREFIX=…` builds; a bundled
`ZangbandTK.app` opens an empty Tk window and closes cleanly; the macOS Tk workflow is
green. Terminal builds unaffected — the other thirteen workflows stay green per DEC-21.

---

### T1 — A window that plays
*First playable ZangbandTK/Tk. The milestone that pays for itself.*

- Term hooks: `Term_text`, `Term_wipe`, `Term_curs`, `Term_xtra` (clear, flush, react,
  event, delay), plus font metrics and resize.
- Keyboard into 4.2's event queue, including modifiers and the arrow/keypad set.
- Multiple term views as **panes in one window** (decision 1) — one main, one messages, one
  recall — since 4.2 already supports subwindows.
- Deliberately **no** menus, **no** native panels, **no** tiles.

**Exit:** a character can be rolled, walked into the dungeon, and killed, entirely in Tk, in
text. Everything 4.2 can draw in a terminal is drawable here.

> **On the pre-game shell.** The original opened a launcher — New / Open / Setup / Quit —
> *before* any game existed, and drove startup from Tcl ([OBS-01](phase3-observations.md)).
> DEC-14 accommodates that without amendment: 4.2 initialises the front end before
> [main.c:579](../../src/main.c#L579)'s `play_game(GAME_SELECT | GAME_NEW | GAME_LOAD)`, so
> there is already a window where Tk is alive and no game has started. T1 uses the
> term-rendered `GAME_SELECT` path; the native launcher lands in T9.

> This is where the phase-2 §5 argument cashes out. From T1 on, the wilderness generator is
> observable. If M4 is being built at the same time, T0–T1 should come *before* it.

---

### T2 — Tiles
*The 16px map from the screenshots.*

- Port the icon engine and `Term_pict`; wire to 4.2's `lib/tiles/list.txt` graphics modes.
- **Default to the neon set (mode 7)** — generated from text shapes, so the PNG is never
  hand-edited.
- macOS `plat` backend: `Tk_PhotoPutBlock` first, measured; Core Graphics only if needed
  (§2.2).
- Transparency, tile scaling, and the double-height/`extra` handling `list.txt` describes.
- Convert the archive's GIFs to PNG and bring the four sets 4.2 has no equivalent for into
  `lib/tiles/`: `pattern16`, `town16/24/32`, `feature16/32`, and the micromap tiles (§1.3).
  These need assignment data written from scratch against our indices — the 2001 tables are
  a reference for artistic intent only.
- **The tile lookup is `index + lighting + frame → rendering`, not `index → tile`.** Three
  findings converge here and must land together, not one at a time: composable modes
  (§6 decision 5), animation frames ([OBS-36](phase3-observations.md)), and **lighting** —
  4.2 supplies four states per grid (`LIGHTING_LOS/TORCH/LIT/DARK`,
  [cave.h:139](../../src/cave.h#L139)) but the term front end handles them by shifting a
  colour attribute, which works for a letter and not for a tile. Generate darkened variants
  by gamma on demand, as the original settled on in its last release
  ([OBS-47](phase3-observations.md)). Night darkens the *wilderness*, so this is not a
  dungeon-only concern.
- **Design the icon layer so a tile lookup may return an *animation*, not just a tile.**
  Zangband's sprites are frame sequences with a delay factor and a ping-pong flag — three
  fields, 74 lines of data, and 4.2 has no equivalent ([OBS-36](phase3-observations.md)).
  Cheap to add now, threaded through everything if retrofitted. Authoring any sprites in the
  first pass is a separate question.
- **The Photo feature and the tileset preview are the same feature** — the original's
  preview specs *are* photo captures, same file format
  ([OBS-20](phase3-observations.md)). Build one and the other is nearly free; store
  monsters and objects by name, not by index.
- **A tileset chooser with a live preview**, settled in [OBS-02](phase3-observations.md):
  rendered through the real `Term_pict` path, from a spec carrying a literal feature grid
  plus monsters and objects placed **by name**. It runs pre-game, which works because
  [main.c:562](../../src/main.c#L562) loads all game data before `play_game()`.

**Exit:** the dungeon renders in tiles at a playable frame rate, mode switchable at runtime.
Judged at actual size, and demonstrated on something worth walking into rather than an empty
corridor.

---

### T3 — The bridge spine
*No new UI. Everything after this depends on it.*

**Build all four seams from §3.2 before any of the read accessors.** They are what every
later milestone is written against, and three of the four are thin wrappers over API that
already exists:

- **Down — the command queue.** One Tcl command that takes a `cmd_code` and named typed
  arguments, over `cmdq_push()` and the seven `cmd_set_arg_*` setters. Not keystrokes:
  ZangbandTK/Tk should never synthesise a keypress to make the game do something.
- **Up — game events.** One C shim registering `event_add_handler` across the 66 event
  types and re-emitting each as a qebind event a script can bind to (§3.3). Written once,
  covers every window that has to redraw when something changes.
- **Sideways — the input hooks.** The override mechanism, plus the discipline that goes
  with it: every hook starts as textui's, and each native dialog later replaces exactly one
  of them. Get the override point right — after
  [`textui_init()`](../../src/ui-init.c#L44), before `play_game()` — and T4–T8 become
  independent of each other.
- **Reads — the `struct` accessor machinery**, and then only the families those milestones
  need, in the order they need them.

Plus:

- **Expose 4.2's command table** — [`cmds_all[]`](../../src/ui-game.c#L386)'s named groups,
  [`struct cmd_info`](../../src/ui-input.h#L44)'s label, keys, `cmd_code` and prereq
  predicate, and `cmd_lookup_key()` for the *live* binding. This is what lets T7 generate
  the menu bar instead of hand-authoring it ([OBS-17](phase3-observations.md)).
- Establish the naming rule from §3.3 as a **written map**: old command → 4.2 concept →
  kept, renamed, replaced by a seam, or dropped. This document is the deliverable, and it is
  what makes T4–T8 estimable. It is also where §3.2's re-scoping gets settled with numbers
  rather than thirds.
- **The scripted-session harness** from §9, which the four seams make possible: a test can
  push a command, wait for an event, and answer a hook without a keyboard or a window.

**Exit:** Tcl can drive the game through the queue, observe the 66 events, answer a prompt
through an overridden hook, and read and write options. `debug.tcl` and `errorInfo.tcl`
work — the debug loop exists before the UI does — and one scripted session runs in CI.

---

### T4 — Player, status and the Misc panel
`player` (36 scripts) and `power`. Delivers the Misc panel, the Progress bars, and the
compact character display.

**Exit:** the three right-hand windows from the screenshots, live.

---

### T5 — Map widget and Micro Map
`cave`, `m_list`, `o_list`, `f_info`. The scrolling map widget and the level overview, with
hover tooltips and the hovered-monster health bar.

- The ten mouse rules from §1.5, as ten testable statements.
- The **linked yellow cursor** both ways between Main and Micro Map.
- Micro Map resolution popup, click-drag scroll, Shift-click to pin.

**Exit:** every Main Window and Micro Map rule in `interface.html` holds, checked against the
original running side by side.

---

### T6 — Knowledge and recall
`r_info` (11), `k_info` (10), `a_info`, `info`, and `describe`. Delivers the Recall window
and the knowledge browsers.

- The Knowledge window's tab set is **4.2's categories plus Pets, Quests and Home** — 4.2
  knows about ego items, runes, features, traps and kill counts that the original had no tab
  for ([OBS-24](phase3-observations.md)).
- The monster grouping is **free**: `monster_base.txt`'s 59 bases and
  [ui-knowledge.c:1159](../../src/ui-knowledge.c#L1159)'s categories already provide what
  `r_info.c`'s 1,737 lines were computing ([OBS-28](phase3-observations.md)).

**Exit:** monster and object recall, and the knowledge menus, are native.

> **The Character window is a five-tab home, not a screen** — Info, Flags, Mutations,
> Virtues, Notes ([OBS-21](phase3-observations.md)). Two of those tabs are where Phase 2's
> M8 output surfaces, and the Notes tab is where 4.2's richer `history_add()` timeline
> belongs. Build the frame in T6; **Virtues can be filled immediately**, since
> [list-virtues.h](../../src/list-virtues.h)'s eighteen virtues and `MAX_*_VIRTUES` already
> exist in the tree ([OBS-22](phase3-observations.md)) — which makes it the cheapest proof
> that the frame works.

> **Preview through the real renderer, never a mock-up.** Three independent instances in the
> original — the tileset preview, the paper doll, and every tab of the Color editor
> ([OBS-29](phase3-observations.md)) — make this a house rule. A preview that goes through
> the actual path cannot lie about the result, and cannot rot when the path changes.

> **Semantic colour roles, and a colour-blind-safe palette.** The Color editor's slots are
> named Good / Info / Bad rather than green / blue / red, which is the property that makes
> recolouring safe ([OBS-29](phase3-observations.md)). Health bars and status flags are
> red-versus-green — the most confused pair there is — so ship validated palettes
> (default, high-contrast, colour-blind-safe) with the per-slot editor behind them. 4.2's
> term build cannot recolour at all, so this is additive.

> **No selectable element without its selector shown.** The rule is broader than menus: the
> store's hover tooltip reads *"g) a Katana"*, spell tables carry their letters, menus carry
> their accelerators ([OBS-48](phase3-observations.md)). A mouse-driven player is taught the
> keyboard continuously, in every list. That is the front end's thesis in one line.

> **Show the whole space, and make the delta unmissable.** The original consistently shows
> what you *don't* have alongside what you do — every equipment slot drawn even when empty,
> all forty flags listed, all four realm books on the Book menu when you own one
> ([OBS-38](phase3-observations.md)). That is the front end's real thesis, and it is what
> accelerators-beside-menu-items are for too: the UI teaches the game's shape rather than
> only reflecting your state. The second half is the standard the original sometimes missed —
> the paper doll's empty box reads instantly, the Flags table's grey wall does not. Check
> every T4–T7 screen against both halves.

> **Copy the affordances, not the layouts.** Twice now, faithful reproduction would have
> reproduced a usability problem rather than a feature: the coupled window group
> ([OBS-16](phase3-observations.md)) and the Flags table, whose greyed-out rows outnumber
> its marks and whose labels sit a screen away from them
> ([OBS-22](phase3-observations.md)). The *ideas* — placeholder equipment slots,
> accelerators beside menu items, the linked cursor, the live character in the score table —
> are what to carry across. The 1999 pixel arrangements around them are not.

> **One universal detail pane, with a typed API.** Recall is not the monster-memory window —
> it is the detail pane for *everything*, driven by whatever the pointer is over, with
> `RecallArtifact` / `RecallMonster` / `RecallObject` / `RecallSpell` / `RecallQuest` / … and
> **25 of 69 scripts feeding it** ([OBS-41](phase3-observations.md)). Build one widget with
> that typed API rather than a detail display per window: one thing to get right, and one
> place for the player to look.

> **Anything disabled is visibly disabled and can say why.** The original greyed spells the
> character could not cast but made the difference invisible, and "cannot cast" has several
> causes — unlearned, level, mana, blindness ([OBS-41](phase3-observations.md)). Native
> menus supply the greying; the detail pane supplies the reason on hover. 4.2 supplies both
> halves: `spell_okay_to_cast()` and `spell_menu_display`'s six states.

> **Every window has a discoverable route.** The original's Pets window — roster, upkeep
> total, a correctly-built command menu — opens only via `p` then `*`, with `*` documented
> nowhere but the prompt string ([OBS-51](phase3-observations.md)). The Window menu lists
> every pane and window that exists; a command with a window behind it says so. One entry per
> window, and it is the difference between a feature existing and a feature being used.

> **Empty states must explain themselves.** The original showed a blank Notes tab when
> note-taking was switched off, with no hint why ([OBS-22](phase3-observations.md)). Same
> failure class as offering menu items the character cannot use: the interface knows what
> the player needs to know and does not say it. Every empty panel says *why* it is empty,
> with the fix one click away.

---

### T7 — The menus
*The milestone the whole front end exists for.*

`inventory` (10), `equipment`, `store`, `home`, `spell`, `mindcraft`, `keymap`, `macro`.
Delivers Choice, the Inven/Book/Action menu bar, stores, and the building services Zangband
adds in phase-2 M5.

- **The menu bar is generated**, from T3's exposed command table — never hand-authored.
- **Every menu item displays its accelerator**, resolved through `cmd_lookup_key()` so it
  follows the player's own keymap. This is the design thesis, not a nicety: the menus teach
  the keyboard rather than replacing it ([OBS-17](phase3-observations.md)).
- **Items grey out via the existing prereq predicates** — `player_can_read_prereq` and
  friends. The original offered impossible actions; we should not.
- **T7 is five reusable widgets, not twenty screens** ([OBS-50](phase3-observations.md)):
  a **transfer window** (Store/Home — verbs, money on/off, owner), a **building window**
  (name, owner, service list), a **generic table** (Book/Powers/Mindcraft/Pets, and arguably Knowledge and
  scores — columns, source, per-row state predicate), the **generic selection pane** (item tester + mode), and the **paper
  doll** (body type). Each parameterised, each mapping onto a 4.2 hook or command.
- **One excellent generic selection pane, plus four bespoke windows — not a dialog per
  command.** The original's unevenness is unfinished work, not a design
  ([OBS-43](phase3-observations.md)): the change log shows commands being converted one per
  release for three years until development stopped. But the fix is not to finish the
  conversion. The test is what the command asks: *choosing from a list* (eat, quaff, read,
  drop, throw) is served completely by a good generic pane; *understanding a structured
  space* (equipment, a realm's books, a store, knowledge) earns a bespoke window because the
  structure is the information. That bounds this milestone instead of leaving it open-ended.
- **The Book window as a sortable five-column table** — Name, Lv, Mana, Fail, Info — which
  is [ui-spell.c:249](../../src/ui-spell.c#L249)'s existing header and
  `spell_menu_display`'s existing six-state colour logic, exposed through the bridge
  ([OBS-40](phase3-observations.md)). Sortable columns and clickable rows come free; so does
  `get_spell_info()`'s damage and duration, which the original never showed.
- **The paper-doll equipment screen** ([OBS-25](phase3-observations.md)) — `inventory2.tcl`,
  2,631 lines, in no document and the strongest thing in the original UI. Empty slots drawn
  as placeholders answer "what could I be wearing?", which a lettered list cannot. Build it
  from **`player->body`**, not a fixed slot list, and key the coordinate table per body
  type — 4.2's slot set is data ([body.txt](../../lib/gamedata/body.txt)), and Phase 2's
  Skeletons and Spectres are exactly the case that breaks a hard-coded doll.
- The **complete menu-bar audit** is in [OBS-44](phase3-observations.md) — all eight of the
  original's menus, leaf by leaf, each mapped to a 4.2 command, a Phase 2 milestone, or a
  front-end feature. `Tool` disappears with the data editors; `Window` mostly disappears with
  the toplevels. Seven menus remain. **That table is this milestone's input.**
- A **declarative menu map** (menu → submenu → command codes) arranges 4.2's code-shaped
  taxonomy into the player-facing one. This is the real design work; the leaf data is free.
  It must be able to **promote commands out of `cmd_hidden`** — that group means "4.2's own
  keyboard menu omits these", not "hide from the player", and it holds six of the ten
  entries on the original's Action menu ([OBS-18](phase3-observations.md)).

> **4.2 supplies the leaves; the arrangement is ours.** Three times now the right move has
> been to take 4.2's declarative table for labels, keys and descriptions, and supply our own
> grouping on top: the menu bar ([OBS-17](phase3-observations.md)), the Knowledge tab set
> ([OBS-24](phase3-observations.md)) and the options categories — where the original's
> eleven categories beat 4.2's five pages, because "when does the game interrupt me?" is not
> the same concern as "how are objects described?" ([OBS-34](phase3-observations.md)). Each
> needs a small declarative map, and anything unmapped falls through to a default so a new
> 4.2 entry can never go missing — only uncategorised.
- **Two accelerator namespaces, kept apart.** Game keys (`^F`, `^P`, `^Q`, `^T`, `^S`, `^X`
  and every letter command) stay literal Control, because they *are* the keymap the menus
  teach. Application commands (Save, Quit, Close Window, Preferences) follow the platform —
  ⌘S, ⌘Q, ⌘W, ⌘, — and on macOS Quit and Preferences belong in the application menu, not
  File. Bind both, display the platform one ([OBS-42](phase3-observations.md)).
- **⌘Q must be safe**, and destructive commands must be named for what they do. "Quit" in
  the original discards the session; 4.2's `Q` *retires the character permanently*. Neither
  may be labelled "Quit" ([OBS-42](phase3-observations.md)).
- **Three key collisions to resolve game-side first**, not here: `p` (Zangband pets vs 4.2
  "start exploring"), `U` (Zangband racial powers vs 4.2 "use an item"), and `Ctrl+T`
  (Zangband time-of-day vs 4.2's *alternate* binding for tunnel — the cheap one to fix).
  An accelerator that displays one thing and does another is worse than none.
- **The keymap editor keeps the keyboard picture and drops the encoding.** Press the chord to
  pick a key, then choose the action from T3's command list rather than typing `\e` into a
  text box ([OBS-31](phase3-observations.md)). Same table as the menu bar — and because
  accelerators resolve through `cmd_lookup_key()`, a rebind shows up in the menus at once.
  Use **4.2's** keymap representation so bindings are portable between the term and Tk
  builds; the original's X11 keysym names were not.
- **Ship zero default macros.** The original's sixteen were a workaround for missing arrow
  and keypad handling — `Control-1..9` for directional alter, arrows for directional run —
  all native in 4.2 ([OBS-32](phase3-observations.md)). Keep the macro *capability*, in
  Preferences, as a power-user convenience rather than a headline.

**Exit:** a full game — shopping, spellcasting, equipping, quaffing, reading, activating —
playable without memorising a single command key. This is the promise in the user's brief.
The Inventory, Recall, Choice, Book and Store rules in §1.5 are the checklist, including the
Recall/Choice hand-off and the grow-on-hover behaviour.

---

### T8 — Birth
`birth-tnb.c` is 3,716 lines, but **less of it is ours to rewrite than that suggests**.
4.2's [ui-birth.c:60](../../src/ui-birth.c#L60) already carries the whole wizard as a state
machine — quickstart, race, class, **realm** (with `REALM_CHOICES 2`, matching Zangband's
Realm 1 / Realm 2), roller choice, point-based, roller, name, history, final confirm — and
`BIRTH_BACK` gives the `< Back` button its semantics for free
([OBS-14](phase3-observations.md)). So T8 is *give native views to stages that already
exist*, not *build a wizard*.

No autoroller: 4.2 removed it and we are leaving it removed. Descriptions come from game
data, never from the old `msgs/birth/en.msg` catalogue, or the Moorcock patrons walk back
into a game whose data already excluded them ([OBS-12](phase3-observations.md)).

Still **last** — but re-check the assumption. M9's realm and book *structure* is already in
the tree: six realms × four books, all twenty-four present in
[class.txt](../../lib/gamedata/class.txt) ([OBS-39](phase3-observations.md)). What may still
move is spell content and balance, not the shape T8's views display. Worth re-reading this
sequencing against M9's real state rather than leaving T8 blocked longer than needed.

**Exit:** graphical character creation covering ZangbandTK's races, classes, realms and
patrons.

---

### T9 — Finish
The **death flow**: tombstone from [lib/screens/dead.txt](../../lib/screens/) with the
character's tile icon added, 4.2's nine-action death menu as native UI, the optional
**Character Record** (dump + message log + photo attached to the score entry — a feature 4.2
lacks entirely), and the Hall of Fame. **`New Game` returns to the launcher; only Quit
quits** ([OBS-46](phase3-observations.md)). `crown.txt` is the winner's screen — nobody will
see it for years, but do not lose it.

Optionally a **music player** — a directory of the player's own files, shuffled, with volume
([OBS-33](phase3-observations.md)). No new dependency: `snd-sdl.c` already uses SDL2_mixer's
`Mix_Music` channel, and shipping no music means no licensing exposure. Off by default.

Sound wired to 4.2's `sound-core.c` using **4.2's own Dubtrain pack**, with the event
vocabulary mapped from `grund.snd` (§1.4); the options UI **generated from
[list-options.h](../../src/list-options.h)'s 56 entries merged with a front-end option
table** — tile set and size, animation, icons in lists, centre-on-player, pane fonts,
palette — presented as one categorised list, so a dead control is structurally impossible
and the player never has to know which layer owns a setting
([OBS-13](phase3-observations.md), [OBS-34](phase3-observations.md)); keymap UI;
help via the Sphinx manual (DEC-17), replacing TkHtml and reusing the `.book`/`.page` tree as its index; the 32 startup tips, rewritten for whatever the UI actually ends up doing; Ttk restyling; zipfs packaging of the Tcl into the
app bundle; and the release `ZangbandTclTK.app`, which by here is T0's minimal bundle grown
up rather than a new piece of work — shipping *beside* `ZangbandTK.app`, which Phase 3 never
touches (§6 decision 14), so the download page has to say in a line which is which.

**Exit:** a double-clickable ZangbandTK.app with no external Tcl dependency.

---

## 6. Decisions needed

**All fourteen are settled.** Raw findings from the running original accumulate in
[phase3-observations.md](phase3-observations.md); the settled rationale for each decision
follows below.

| # | Question | Recommendation | Blocks |
|---|---|---|---|
| ~~1~~ | ~~One window with panes, or many toplevels?~~ | **Settled — panes.** See below. | — |
| ~~2~~ | ~~The four data editors~~ | **Settled — dropped.** ZangbandTK ships no data editors. See below. | — |
| ~~3~~ | ~~Isometric view~~ | **Settled — dropped**, files kept. See below. | — |
| ~~4~~ | ~~Tile assignment~~ | **Settled — 4.2's.** See below. | — |
| ~~5~~ | ~~Composable graphics modes~~ | **Settled — adopt composability.** See below. | — |
| ~~6~~ | ~~Replace `tk/library/` with Ttk?~~ | **Settled — Ttk, themed by platform.** See below. | — |
| ~~7~~ | ~~Revive the Sound Window's per-monster tier?~~ | **Settled — no.** See below. | — |
| ~~8~~ | ~~What renders the in-game help?~~ | **Settled — Sphinx content, rendered in-app.** See below. | — |
| ~~9~~ | ~~Term, native, or canvas-styled-classic?~~ | **Settled — native widgets where possible.** See below. | — |
| ~~10~~ | ~~Release target?~~ | **Settled — macOS, Windows and Linux: whatever Tk supports.** See below. | — |
| ~~11~~ | ~~Localisation?~~ | **Settled — none. English only.** See below. | — |
| ~~12~~ | ~~Where do the toolchain, the C and the scripts live?~~ | **Settled — `tcltk/`, `src/tcl/`, `lib/tcl/`.** See below. | — |
| ~~13~~ | ~~Which platform first?~~ | **Settled — macOS through development; the other two before release.** See below. | — |
| ~~14~~ | ~~Does the Tk build replace `ZangbandTK.app`?~~ | **Settled — no. Two applications, permanently.** See below. | — |

**Settled — Ttk, themed by platform.** Confirmed by project owner. Tk 9's native widget set
replaces `tk/library/`'s **10,636 lines** of 1999 megawidgets — `ttk::notebook` for tabbed
frames, `ttk::treeview` for list canvases, `ttk::progressbar`, `ttk::spinbox`, real toolbars.

The decisive property is the one the old set can never have: **Ttk widgets are themed by the
platform**, so on macOS they look like macOS. The hand-drawn megawidgets look like
Windows 98 everywhere, permanently, because they draw their own bevels. With #10 making
Windows and Linux deliverables, that is no longer a cosmetic preference.

*Execution is incremental, not a big-bang port:* Ttk for everything touched from T4 onward,
and `tk/library/` shrinks to nothing as its consumers are replaced. Nothing is ported for
its own sake.

*One caveat to watch:* `ttk::treeview` is less flexible than a hand-drawn canvas list, so
tile-heavy rows — Knowledge with an icon per entry
([OBS-24](phase3-observations.md)) — may still want a custom canvas item. That is a
per-widget call at the time, not a reason to keep the old toolkit.

**Settled — native widgets where possible.** Confirmed by project owner. The preference
order for any screen is:

1. **Native Ttk** — the default, and the answer for anything list-shaped, sortable or
   form-like.
2. **Canvas-styled-classic** — only where the classic layout genuinely *is* the best layout.
3. **Term** — a stopgap for what we choose not to rebuild, never a target.

This narrows the middle tier considerably from
[OBS-15](phase3-observations.md)'s framing, which is the intent.

> **One case worth arguing at T6 rather than assuming now.** The character sheet is the place
> where canvas-classic has a real claim: it is the most recognisable artefact in the game,
> fluent players read it at a glance, and the original spent ~1,850 lines reproducing it
> precisely so it could stay classic *and* become clickable. A native re-layout would be
> better information design and would cost those players something real. Flagged as a T6
> decision under this policy, not an exception to it.

**Settled — composable graphics modes.** Confirmed by project owner. The original derived
twelve icon sets from ~3 orthogonal axes — creatures as tiles *or* ASCII, three terrain
layers, three sizes — from four image files
([OBS-02](phase3-observations.md)). Seeing the ASCII hybrid run settled it: it is a legible
mode a letters-reader would choose, not a degraded fallback. 4.2's one-sheet-per-mode model
is replaced.

This is the third thing converging on the same code, and all three land together in T2:
**the tile lookup becomes `index + lighting + frame → rendering`**, composable across
creature/terrain/size axes, with animation frames
([OBS-36](phase3-observations.md)) and runtime-generated darkened variants
([OBS-47](phase3-observations.md)).

**Settled — no data editors.** Confirmed by project owner: *"a very 1990s thing."* Vault,
Assign, Alternate and Sound go — ~9,000 lines. Mapping data lives in `lib/gamedata/` and
`lib/customize/` where it can be diffed and reviewed.

**Settled — tile assignment is 4.2's.** `list.txt` + `graf-*.prf`. The archive's unique sets
(town, feature overlay, pattern, micromap) still need assignment data written against our
indices — that half is work either way.

**Settled — no isometric view.** `widget-iso.c` (1,656 lines) and `canv-widget.c` stay in the
tree unported. They double the riskiest part of the Tk 9 port for a mode few used.

**Settled — help is Sphinx content, rendered inside the application.** Confirmed by project
owner: in-app, not a browser hand-off. [docs/](../../docs/) is already a working Sphinx
project with **37** chapters.

> **How, given TkHtml is dead.** Do not render HTML. Build the manual with a **text-oriented
> Sphinx builder** and draw it into a **Tk text widget**, which natively supports fonts,
> colours, images, embedded widgets and tag bindings for cross-reference links. That gives
> in-app help that is searchable, themed with the application, and dependency-free — no
> TkHtml, no Tkhtml3, no webview.
>
> The `.book`/`.page` tree from `tk/doc/contents` ([§1.5](#)) becomes the contents sidebar,
> and Sphinx's own toctree generates it rather than a hand-maintained file.
>
> Keep "open the full manual in a browser" as a *secondary* action for anything the text
> widget renders poorly — wide tables, diagrams. Secondary, not the primary path.

**Settled — release target is macOS, Windows and Linux: whatever Tk supports.** Confirmed by
project owner. This is firmer than DEC-21, which deferred Windows and Linux to "a later
date". Portability stops being insurance and becomes a deliverable.

> *What changes:* CI must build the Tk front end on all three, not just keep the terminal
> builds green. The `plat` layer needs three real backends rather than one plus intentions
> ([§2.2](#)). And platform conventions — the accelerator namespaces in
> [OBS-42](phase3-observations.md), the application menu, native file dialogs — need doing
> per platform rather than macOS-first.
>
> *What does not change:* DEC-22 still applies — macOS means Apple Silicon only. And
> "supported" should mean *built and smoke-tested* on all three; deep behavioural testing can
> stay macOS-led without weakening the claim.

**Settled — one window, Ttk panes.** Confirmed by project owner: *"the window model - use
panes."*

The original built one composed window out of toplevels because Tk 8.0 gave it no choice —
`wm transient` on ~40 windows, a 317-line central geometry authority, and layout bands at
800px ([OBS-16](phase3-observations.md), [OBS-37](phase3-observations.md)). We express that
intent directly instead of reproducing the workaround.

*What follows from it:*

- **The `Window` menu mostly disappears.** `Arrange Windows` and `Maximize Windows` manage a
  problem panes do not have — and `Arrange` is the feature that most visibly broke on a
  modern screen. What survives is pane visibility toggles.
- **Layouts are saved automatically**, as `AutoSave Positions` already did by default. The
  explicit form is **named layouts** the player can switch between, which beats one
  `Save Window Positions` menu item and is the same 317 lines' worth of concept applied to
  sashes.
- **A hidden pane folds into its neighbour rather than vanishing** — the original's own
  precedent, from the 2.4.0r5 change log: *"When the Message Window is closed, it is
  integrated into the top of the Main Window."*
- **Fonts are per pane** ([OBS-30](phase3-observations.md)), and a term pane's font sets its
  grid, so its font control belongs beside its tile-size control rather than in a list.
- **The map pane owns its four overlay planes** — tiles, the linked cursor, status badges,
  and the hover name-and-health bar ([OBS-47](phase3-observations.md) neighbours).
- **Detachable panes are post-T7**, not T1 scope. Pane today, toplevel tomorrow is the modern
  compromise and Tk can do it, but it is real work and nothing depends on it.

*What it does not change:* the multi-monitor case is genuinely lost until detaching exists.
That is the accepted cost, and it is the right trade for a game whose windows were only ever
meant to be read together.

**Settled — no per-monster sound reassignment.** Confirmed by project owner: *"do not allow
players reassign sounds, no one will use it anyway."*

`grund.snd` bound sounds to 672 individual monsters and the Tcl Sound Window let players
rebind them live. Reviving that would mean adding a per-monster tier to 4.2's
`sound-core.c`, which is flat and message-driven, and then a UI to edit it. Both are
dropped.

*What this rules out:* the Sound Window's assignment editor, the per-monster tier in
`sound-core.c`, and any attempt to carry the 672 index-keyed bindings forward. T9 maps the
211 symbolic events onto 4.2's 149 and stops there. `sound.tcl` (2,705 lines of C behind it)
shrinks to a volume and on/off control.

*What it does not rule out:* the symbolic monster-spell and martial-art events. Those are
names, not indices, and they are the part of the old vocabulary worth having.

**Settled — no localisation. English only.** Confirmed by project owner. The archive ships
34 message catalogues in English *and* Japanese, and `msgs/` is listed in §1.5 as an asset
because it is one — but it is an asset for a project with translators, and this one has
none.

This is a decision that has to be taken at T0 rather than discovered at T9, which is the
only reason it is worth a paragraph. Every widget from T4 on either routes its strings
through a catalogue or does not, and retrofitting the first answer across a finished UI
means touching every string in it. Write English literals; do not wrap them in `msgcat`;
do not carry `msgs/` forward. The one thing to keep is the *habit* the catalogues enforced —
no string built by concatenating fragments — because that is good practice regardless and
it is the part that would make a future translation possible rather than impossible.

**Settled — three homes, and each one follows an existing pattern.** The question is where
the toolchain, the ported C and the 121 scripts live now that `archive/` is staying
untracked.

| What | Where | Tracked? | Why there |
|---|---|---|---|
| Tcl/Tk 9.0.4 sources | [tcltk/](../../tcltk/) | **Yes** | CI cannot see `archive/`, and a toolchain that exists on one Mac is not a build |
| What `build-tcltk` produces | `tcltk/build`, `tcltk/local` | No | Reproducible in two minutes; gitignored |
| Ported widget library and bridge | `src/tcl/` | Yes | It is C we compile, and it sits beside the other front ends |
| The Tcl UI | `lib/tcl/` | Yes | It is data the game loads at runtime, like `lib/tiles/` and `lib/screens/` |

`lib/tcl/` is the one worth arguing, and the argument is that it buys the whole of 4.2's
existing path machinery for three lines: a `char *ANGBAND_DIR_TCL` beside the other
thirteen at [init.h:310](../../src/init.h#L310), its `BUILD_DIRECTORY_PATH` beside the
others at [init.c:355](../../src/init.c#L355), and its `string_free`. Install rules,
`SHARED_INSTALL`, `READONLY_INSTALL`, the self-contained build and the packaging scripts
then all handle it already, because they handle `lib/` subdirectories generically. Putting
the scripts anywhere else means teaching every one of those about a new kind of thing.

*And `archive/` stays exactly as it is* — gitignored, untracked, the reference copy. Nothing
is built from it. When the port needs something out of it, that thing is copied into the
project and tracked, which is what happened to Tcl/Tk on 11 Sep and is the precedent for
the widget library, the four tile sets in T2, and `grund.snd`'s vocabulary in T9.

**Settled — macOS through development, all three before release.** Confirmed by project
owner, and it narrows decision 10 rather than reopening it: the *deliverable* is still
macOS, Windows and Linux. The *sequence* is macOS until the front end is finished, because
that is the only platform being tested by hand.

*What follows from it:*

- **One CI job at T0, not three.** `tk.yaml` builds macOS. Linux and Windows jobs land when
  there is something worth keeping green on them — and they will find real breakage when
  they do, which is the point of adding them before release rather than at it.
- **Portability stays a coding discipline meanwhile**, per DEC-12's four rules. The `plat`
  layer gets its macOS backend written and its other two stubbed with `#error`, exactly as
  the original did — a stub that fails loudly at compile time is honest; a silently wrong
  fallback is not.
- **The two accelerator namespaces still get separated at T7** even though only the macOS
  half is testable, because the separation is a data-structure decision
  ([OBS-42](phase3-observations.md)) and merging them would be the expensive mistake.

**Settled — two macOS applications, permanently.** Confirmed by project owner: *"we want to
keep the existing build because this is a different play experience. The TK is a separate
release … the current version remains, it is built and will always be playable."*

`ZangbandTK.app` — `main-cocoa.c`, built by `gmake -f Makefile.osx`, identifier
`org.zangbandtk.zangbandtk` — is not a stepping stone to anything and is not deprecated by
this phase. The Tk front end ships beside it as its own application, provisionally
**`ZangbandTclTK.app`** with its own bundle identifier; the name is cheap to change up to
T9 and impossible to change afterwards without orphaning everyone's Dock icon.

*Why this is the better answer, and not merely the owner's answer:* the two are genuinely
different games to sit down to. One is a terminal the player reads; the other is a menu bar
they browse. Replacing the first with the second would take something away from the player
who already prefers it, in exchange for nothing they asked for — and it would put every
Phase 3 milestone on the critical path of an application that currently works.

*What follows from it:*

- **`Makefile.osx` is never touched by Phase 3.** The Tk build is a cmake target from T0
  onward, and the two build systems do not meet.
- **Distinct bundle identifiers**, or the two fight over Launch Services registration,
  preferences and the icon the Dock shows.
- **Shared savefiles, deliberately.** Both write `~/.angband/ZangbandTK` — same
  `ANGBAND_DIR_USER`, same characters, same scores. A character started in the terminal and
  continued in Tk is a feature, and it is the same relationship the terminal build already
  has with the Cocoa one. Nothing in the front end may write a savefile the other cannot
  read.
- **The release ships both**, which makes the naming decision a user-facing one rather than
  a build detail: the download page has to say in a line which is which.
- **DEC-21's "the terminal build stays the reference" gains a second referent.** Any
  divergence in behaviour between the two applications is a bug in the newer one.

---

## 7. Risks

| Risk | Severity | Handling |
|---|---|---|
| **`widget1/2-dll.c` against Tk 9 internals.** 203 of 253 remaining errors; Tk's internals changed substantially since 8.3. | **High** — the one place this could genuinely stall | Timebox it in T0. If it resists, the fallback is a Tk canvas plus Ttk instead of custom canvas items — slower, and **not free**: it means reimplementing the four custom item types (`cursor`, `progressbar`, `text`, `rectangle`), one of which draws the linked cursor in §1.5. Decide by measurement, not by attachment to the original. |
| **The bridge rewrite is the bulk of Phase 3.** Smaller than the 26,000-line estimate now that three of §3.2's four seams already exist, but the read accessors are untouched by that. | **High**, but linear and testable | Build the four seams first (T3), then sequence the accessors by measured script demand (§3.3) so each family lands with the scripts it unlocks. Never "rewrite `interp1.c`" as one task. |
| **Aqua has no fast blit path.** | Medium | `Tk_PhotoPutBlock` first, measured at actual size. Behind `plat`. |
| **Phase 2 keeps moving under it.** M7/M9/M10 change races, classes, realms and pets — exactly what T4 and T8 display. | Medium | T8 after M9. T4 reads through accessors, not layouts. |
| **108,000 lines of unfamiliar Tcl.** | Medium | `debug.tcl` and `errorInfo.tcl` land in T3, before the UI work. Adapt scripts as their command family lands, never speculatively. |
| **Two front ends diverging.** | Low | The term build stays the reference. No gameplay logic in `src/tcl/` — ever. |
| **The input hooks are declared but unexercised.** No front end in the tree overrides one; T3 is the first (§3.2). | Medium | Override one hook early — `get_check_hook` is the smallest — and prove the round trip before T7 depends on sixteen more. |

---

## 8. What has not been verified

> **✅ The original runs.** Confirmed by the project owner on this Mac, with the Tips window,
> the six-window layout, the linked yellow cursor and the TkHtml help browser all live. That
> retires the working assumption behind much of §1 — that the program was only recoverable
> from screenshots.
>
> **This is worth more than any single finding in this document.** A running reference means
> every behavioural question in T3–T8 has an oracle: rather than reading `interface.html` and
> guessing, run both and compare. Specifically it makes testable the things static reading
> cannot settle — event ordering and what `inkey_flags` actually holds at each prompt, the
> Recall/Choice hand-off, redraw timing, and where the Micro Map's cursor tracking hooks in.
>
> Two things to capture from it while it runs, because they are cheap now and expensive
> later: **screen recordings of each interaction in `interface.html`**, and the **contents of
> the Sound, Assign, Color and Options preference windows**, which exist only as live UI.
>
> **How it runs, for the record:** the original **Windows** binary under **CrossOver**, with
> a Tcl/Tk **8.3.3 build from 2001**. Version-specific — a current Tk download does not work,
> which matches the `package require` pins and the private-header use in §2.2.
>
> One caveat that follows from that, and it is not a small one: CrossOver means the reference
> is exercising the **`PLATFORM_WIN`** code path — the Windows DIB blit, `tkWinInt.h`, the
> DirectSound/WaveMix layer. Behaviour observed there is the *Windows* behaviour. For most of
> `interface.html` that is irrelevant, since the rules are Tcl-level. It is not irrelevant for
> redraw timing, blit performance, or anything touching `plat.c` — so do not use the reference
> to set performance expectations for the Aqua path.

Stated plainly, so nothing here reads as more settled than it is:

- **Nothing has been linked.** §2.2 is `-fsyntax-only`. The toolchain it would link against
  now exists and is verified (§2.1), but no game object has been built against it, and link
  errors and runtime behaviour are unmeasured.
- **The 121 scripts have not been run against Tk 9.** They *have* now been run against their
  own Tk 8.3.3 — see the box above — but 8.3 → 9 changes scripts too (encoding, the
  `Tcl_Size`-driven API surface, removed commands), and 108,000 lines are unaudited.
- **The input hooks are unexercised.** They are declared, and `textui_input_init()` installs
  them, but no front end in the tree overrides one. T3 is the first (§3.2).
- **Rendering performance is unmeasured** on the portable blit path. This is the one that
  could change T2's design.
- **The read-accessor surface has not been mapped** to 4.2 concepts. §3.2 settles which
  third of the old bridge disappears; it does not size what remains. Until T3 produces that
  map, T4–T8 sizes are informed guesses.
- **Windows and Linux are untried** for the front end, deliberately, per §6 decision 13 —
  which now says *when* they get tried rather than leaving it open.
- **The graphics are audited for presence, not for fitness.** Every asset is there and every
  file decodes (§1.3), but no tile has been mapped to a 4.2 index, and nothing has been
  judged at actual size in a running window — which is the only judgement that counts.
- **The sound event mapping is sketched, not written.** §1.4 confirms 7 exact name matches
  and 11 obvious truncations; the remaining ~190 symbolic events have not been walked
  against 4.2's list one by one.

---

## 9. How this gets tested

The plan turns `interface.html` into "ten testable statements" (§1.5) and sets T5's and T7's
exits as *checked against the original running side by side*. That is a person with two
windows open, and it does not survive being done twenty times.

It should not have to. **This is the most testable front end the project will ever have**,
and for a reason peculiar to it: the UI is written in a scripting language, and T3 exposes
the game to that language deliberately. `main-gcu.c` and `main-cocoa.c` can only be driven
from outside, through a pseudo-terminal, one keystroke at a time — which is exactly what
[scripts/smoke-tty](../../scripts/smoke-tty) does and why it is 150 lines of pty handling.
A Tk build can be driven from *inside*, in the same language the UI is written in.

Four layers, cheapest first. Each lands with the milestone that makes it possible.

| Layer | What it asserts | Lands | Runs |
|---|---|---|---|
| Toolchain check | The pieces `build-tcltk` installed are the pieces the port needs — private headers, config files, zipfs | ✅ done | Every CI run, first step |
| `tcltest` unit tests | Individual Tcl procedures: the menu map, the tile lookup, the contents tree, formatting | T0 skeleton, real tests from T4 | Every CI run |
| Scripted sessions | The game plays: push a command, wait for an event, answer a hook, assert what changed | T3 | Every CI run |
| Synthesised interaction | The ten mouse rules, the Recall/Choice hand-off, grow-on-hover | T5, T7 | Every CI run if the runner allows a display; otherwise a pre-release gate |

**The scripted session is the one that matters**, and §3.2's four seams are what make it
possible. A test pushes `CMD_WALK` with a typed direction, waits for `EVENT_PLAYERMOVED`,
and reads the new grid — no keyboard, no window, no screen scraping. The same harness
answers prompts by overriding a hook from Tcl, which means a test can play through a store
purchase or a spell cast without a single synthetic keystroke. `scripts/smoke-tk`, beside
`smoke-tty` and `borg-smoke`, is the shape: reach the dungeon, assert the map drew, exit
non-zero with the last state if not.

**Synthesised interaction is Tk's own trick.** `event generate` delivers a real
`<Button-1>` to a real widget through the real binding, so *"Control-left-click is
`Alter (+)`"* becomes an assertion rather than a checklist line. Write each of the ten rules
as one test as its rule is implemented, and T5's exit condition stops being a person with
two windows open and becomes a suite that stays true afterwards.

**Three constraints from how this repo already works**, so the harness fits rather than
fights:

- The unit tests in [src/tests/](../../src/tests/) are C programs linked against
  `angband.o`, they are **not** in ctest, and the test front end is off by default. The Tcl
  suites are a *second* body of tests with a different runner; do not try to merge them.
- **macOS has no `timeout`.** Every harness here needs its own watchdog, as `smoke-tty`
  already does — a hung Tk event loop otherwise hangs the job rather than failing it.
- A Tk test needs a display. Aqua on a CI runner is the open question: if it turns out the
  runner cannot open a window, the first three layers still run headless and only the fourth
  moves to a local pre-release gate. Find this out at T0, when the answer costs nothing.

**What cannot be asserted this way is the rendering**, and it should not be faked. Tiles are
judged at actual size, in a running window, on something worth walking into — no golden-image
comparison substitutes for that, and a pixel-diff suite over a tileset still being authored
would fail constantly for reasons nobody cares about. T2's photo capture
([OBS-20](phase3-observations.md)) is the right tool: capture on demand, for a person to
look at.
