# Phase 3 — Observations from the running original

**Companion to** [phase3-tcl-tk-frontend.md](phase3-tcl-tk-frontend.md). The plan is the
stable document; this is the log.

**The reference build.** ZAngbandTk 2.4.0r5, the original **Windows** binary, running under
**CrossOver** with a Tcl/Tk **8.3.3 build from 2001**. Version-specific — a current Tk
download does not work, which matches the `package require` pins and the private-header use
in [§2.2](phase3-tcl-tk-frontend.md).

> **Standing caveat on everything below.** CrossOver means this is the **`PLATFORM_WIN`**
> code path: the Windows DIB blit, `tkWinInt.h`, DirectSound/WaveMix. Tcl-level behaviour
> transfers; redraw timing, blit performance and anything touching `plat.c` does not. Never
> use the reference to set performance expectations for the Aqua path.

Each observation records what was seen, what it means, and what it changes. Findings that
settle something get folded into the plan and marked **→ folded**.

---

## OBS-01 — A startup launcher runs before the game exists
*Screenshot: `New` / `Open` / `Setup` / `Quit` over the Tcl Powered logo.*

The window is `init-startup.tcl` (1,875 lines); the logo is `pwrdLogo175.gif`, already
catalogued. `New` calls `angband game new`; `Open` opens `choose-game.tcl` (955 lines),
which lists savefiles from `lib/save` with a character description per entry and a Browse
button. **Tcl drives game startup** — the play loop begins only when the launcher says so.

**Why this looked like a problem for DEC-14.** DEC-14 makes `main-tcl.c` a term backend with
the game owning its main loop, which seems to leave no room for a UI that runs *before* a
game exists.

**It isn't one.** 4.2's [main.c:579](../../src/main.c#L579) already does:

```c
play_game((select_game) ? GAME_SELECT : ((new_game) ? GAME_NEW : GAME_LOAD));
```

The front end is initialised *before* that call, so there is a window in which Tk is alive
and no game has started. That is exactly where the launcher goes, and the three buttons map
onto modes 4.2 already has. `main-win.c:3324` does the same thing — set `savefile`, then
`play_game(GAME_LOAD)`.

**And it deletes work.** `choose-game.tcl` needs savefile metadata without loading the game,
which is what `savefile-z.c` (1,240 lines) existed to do. 4.2 has that as public API:
`savefile_getter`, `got_savefile()`, `get_savefile_details()` returning
`{fnam, desc, foff}` ([ui-game.h:50](../../src/ui-game.h#L50)), used by `list_saves()`.
So **`savefile-z.c` is dropped, not rewritten.**

→ **folded**: plan §4 moves `savefile-z.c` from Rewrite to Drop. T1 gains a term-rendered
`GAME_SELECT` path; the native launcher lands in T9.

---

## OBS-02 — `Setup` chooses the tileset before the game starts, with a live preview
*Screenshots: Setup → Icons, showing a rendered dungeon scene above a set list.*

Four tabs — **Icons, Music, Sound, Variant** — reached from the launcher, so all of it is
pre-game front-end configuration, not game state. The Icons tab lists twelve sets:

| | 16×16 | 24×24 | 32×32 |
|---|:-:|:-:|:-:|
| Adam Bolt | ✓ | ✓ | ✓ |
| Adam Bolt + Original Features | ✓ | | ✓ |
| Ascii + Adam Bolt Features | ✓ | ✓ | ✓ |
| Ascii + Original Features | ✓ | | ✓ |
| David E. Gervais | | | ✓ |
| Isometric David E. Gervais | | | ✓ |

**This resolves the `classic` puzzle in the configs.** `classicNN.cfg` loads `adam16.gif`
yet is named "classic", which made no sense on paper. In the UI, `classic` is **"Ascii +
… Features"**: ASCII letters for monsters and objects, *real tiles for terrain*. The
screenshot shows a yellow `@` and a red `p` standing on Adam Bolt stonework. The `ascii`
icon type in the `.vlt` header row is what draws the letters.

**The important structural finding: the original's graphics modes were composable, and
4.2's are not.** Twelve sets come from roughly three orthogonal axes —

- **creatures/objects**: Adam Bolt tiles *or* ASCII
- **terrain**: none, `adam16_feature.gif`, or `feature16/32.gif` ("Original Features")
- **size**: 16 / 24 / 32

— which is why four image files yield twelve modes. 4.2's `lib/tiles/list.txt` is
monolithic: one sheet per mode, five modes, no separable terrain layer. So the "+feat"
overlay from plan §1.3 is not merely nicer walls; it is **an orthogonal axis 4.2 does not
have**, and it is what makes an ASCII-creatures/tiled-terrain hybrid possible at all.

That hybrid is worth wanting. It is the honest answer for a player who reads monsters
faster as letters but wants the dungeon to look like a place.

**The preview is a live render, not a still.** `preview-adam24.txt` is a render spec:

```
gwidth: 24 gheight: 24
xmin: 40 xmax: 57 ymin: 45 ymax: 56
47 52 object 178 "You see a Scroll titled "banmur dan evs""
50 50 monster 46 "You see a Novice mage (sleeping)"
```

A named map region, at a named tile size, with objects and monsters placed — pushed through
the actual icon pipeline. Copy this design: a preview rendered through the real path cannot
lie about what you are going to get. (Pre-rendered `preview-*.gif` stills exist too, one per
set, so the shipped build may have used those; the `.txt` shows the intent.)

**Settled — the preview is a live render.** Confirmed by project owner: *"yes we should do
the live render."* Rendered through the real `Term_pict` path into an offscreen surface, then
into the Tk photo. A preview that goes through the actual pipeline cannot lie about what you
will get, and it cannot silently rot when the pipeline changes.

Two consequences worth settling now rather than discovering in T2:

1. **It needs game data, and that is available in time.** `monster 46` and `object 178` are
   `r_info`/`k_info` indices, so nothing renders until the data files are parsed — and Setup
   runs pre-game. That is fine: [main.c:562](../../src/main.c#L562) runs
   `init_display(); init_angband(); textui_init();` and only *then* `play_game()`. So in the
   pre-game window Tk is alive and all game data is loaded, with no player and no cave. That
   is exactly enough.

2. **But the scene must not sample a cave.** The original's spec names a map *region*
   (`xmin: 40 xmax: 57 ymin: 45 ymax: 56`) — it reads a real level, which pre-game does not
   have. Ours needs a **literal feature grid in the spec**, plus monsters and objects placed
   on it **by name, not index**, so it survives Phase 2 renumbering. An ASCII grid plus
   placements is the same shape as the town templates' `D:` rows — a format this codebase
   already parses.

**Still open — the composability question is *not* settled by the above.** Does ZangbandTK
adopt the orthogonal model (creatures × terrain × size) or keep 4.2's monolithic modes and
just add the archive's sets? Composability costs a redesign of the icon layer; monolithic
costs the ASCII-creatures-on-tiled-terrain hybrid. Plan §6 decision 3, and it still blocks
T2's tile work.

---

## OBS-03 — Music is BASS, and only BASS
*Screenshot: Setup → Music. One radio button, "Requires DirectX 3 or above."*

Confirms what the source implied. Music was BASS-only, so it was Windows-only and
DirectX-dependent, and BASS is a non-commercial-licensed third-party library. The tab's own
warning — *"if the game crashes a few seconds after starting, you should disable music"* —
is its own recommendation.

→ **folded**: already in plan §1.6 as a dependency that must stay out. No music in T9.

---

## OBS-04 — Sound offers three backends, all of them Windows
*Screenshot: Setup → Sound. BASS (samples) / DirectSound / WaveMix.*

The seven pluggable backends in the source are not seven *usable* backends: the shipped
Windows build exposed three, all Windows-only, and the choice existed because Windows audio
in 2001 was a mess.

This strengthens plan §4: drop the whole backend layer and 2,705-line `sound.c`, and expose
4.2's `sound-core.c` + `snd-sdl.c`. **There is no library choice to reproduce** — the Music
and Sound tabs collapse into one preference with a volume and an on/off.

→ **folded**: plan §4 and T9 already say this; the observation removes the last doubt.

---

## OBS-05 — The `Variant` tab hosts more than one game
*Screenshot: Setup → Variant, listing `AngbandTk-292r2` and `ZAngbandTk-240r5`, with an
"Always choose this variant" checkbox.*

The multi-game launcher, confirmed live: one install hosting several *bands, with a
remember-my-choice option. This install carries two; the framework supported four
(AngbandTk, KAngbandTk, OAngbandTk, ZAngbandTk). This is exactly what `src/common-dll`, the
stub generator and `main-boot.c` existed for, and why the Tcl was parameterised by
`[variant ZANGBANDTK]` — a pattern that shows up throughout the scripts.

**Settled — not implemented.** Confirmed by project owner: *"not something we will
implement. we can skip that."*

*What this rules out:* the Variant tab, `main-boot.c`, the DLL split and stub generator, and
the `variant/<name>/` directory layout the archive uses to keep per-variant `config`, `doc`
and `image` apart.

*What it does not rule out:* leaving the `[variant …]` guards in adapted scripts where
deleting them is more churn than keeping them. They are harmless once only one variant
exists — just do not let new ones be written.

→ **folded**: plan §4 already drops the DLL split; this settles the UI half too.

---

## OBS-06 — 2.4.0's world is authored; ours stays generated
*Noted from the vault editor, confirmed in `w_info.txt`.*

→ **folded** as plan §3.3, with the full comparison, the ten-line world map, and the three
consequences for Phase 3. **Settled: the wilderness stays generated.**

---

## OBS-07 — The interaction model was written down
*Found while chasing the Tips window.*

`tk/doc/interface.html` is an 11 KB interaction specification: ten numbered mouse rules plus
per-window behaviour. `tips.txt` adds 32 one-line tips, each naming one affordance.

→ **folded** as plan §1.5. Both are T5 and T7 acceptance criteria.

---

## OBS-08 — The help system is TkHtml, not WinHelp

→ **folded** as plan §1.5 and §6 decision 5. TkHtml is dead, unbuildable against Tk 9, and
LGPL; the help window renders the Sphinx manual instead, keeping the `.book`/`.page` tree
as its index model.

---

## OBS-09 — The linked yellow cursor

Hovering the Micro Map moves a yellow rectangle in the Main Window to the matching grid, and
the link is reciprocal. A `cursor` canvas item at `main-window.tcl:1503` and
`misc-window.tcl:426`, implemented in `widget2-dll.c` — the file holding 155 of the 253
remaining Tk 9 errors.

→ **folded** as plan §1.5 and the §7 risk row.

---

## OBS-11 — `New` opens the birth wizard
*Screenshot: Character Creation — "Choose your character's race", scrolling race list with a
live description pane, and Options / < Back / Next > / Quit.*

Confirms the shape `birth-tnb.c` (3,716 lines) implements: a multi-step wizard with Back and
Next, a list on the left, and a description pane on the right that tracks the selection. Not
a term screen. `Options` from inside birth reaches the birth options
(`birth-options.tcl`), so options are settable mid-creation.

Visible races, alphabetical: Imp, Klackon, Kobold, Mindflayer, Nibelung, Skeleton, Spectre,
Sprite, Vampire, Yeek, Zombie — which overlaps heavily with what is being added to the game
right now (`b4eac5011` Skeleton/Zombie/Spectre/Ghoul, `56576b94a` Barbarian/Klackon/
Nibelung/Imp). **No Ghoul in 2.4.0's list**, worth checking against 2.7.5 before assuming
it is a Zangband race at all.

**Two things to take from it:**

1. **The race description prose is a source.** The Spectre entry runs to a full paragraph on
   half-corporeality, passing through walls at a cost, the eldritch howl, and nether
   resistance feeding its form. That is Zangband's own flavour text for races landing in the
   game this week — worth reading against what is being written now, and DEC-16 already
   makes the official documentation a first-class requirements source.
2. **It reinforces T8's position, last.** This wizard displays races, classes, realms and
   patrons — precisely what M7 and M9 change. Building it before those settle means building
   it twice.

---

## OBS-12 — The class step, and where the old flavour would sneak back in
*Screenshot: Character Creation — "Choose your character's class", same two-pane wizard.*

Classes visible: Chaos-Warrior, High-Mage, Mage, Mindcrafter, Monk, Paladin, Priest, Ranger,
Rogue, Warrior, Warrior-Mage.

The Chaos-Warrior description names its patrons: *"Every Chaos Warrior has a Patron Demon,
such as **Arioch**, **Mabelode** or **Balaan**."* Those are Moorcock's Lords of Chaos, and
the earlier Character Info plate showed `Patron: Mabelode`.

**This is already fixed in the game data, and that is exactly why it is worth logging.**
[lib/gamedata/patron.txt](../../lib/gamedata/patron.txt) carries nine patrons and every one
is Amber's Courts of Chaos: **Swayvill, Suhuy, Mandor, Dara, Gramble, Jurt, Despil, Borel,
Gilva**. The decision log settled the roster (see the DEC on Zangband's sixteen Moorcock
patrons). Nothing to redo.

**The risk is the prose, not the data.** These descriptions do not come from game data at
all — they live in
[`tk/msgs/birth/en.msg`](../../archive/Tk/CommonTk-1.4/OmnibandTk/tk/msgs/birth/en.msg),
1,026 lines and 119 catalogue entries of hand-written race and class text with the old
flavour baked in. Adapt the birth scripts and reuse that catalogue as-is, and Moorcock walks
straight back in through the front door of a game whose data already excluded him. The same
applies to every Lovecraft and Tolkien reference DEC-01 treats as drift rather than legacy.

**So the rule for T8: do not translate that catalogue — replace it with generated text.**
`patron.txt` already has a `patron-desc:` field, and 4.2 carries its own race and class
descriptions. Read descriptions from game data and the wizard cannot drift from what the
game actually does. Keep the *catalogue mechanism* — it is what makes the Japanese
localisation possible — and let the content come from the data.

**And it reinforces T8 going last, again.** 119 entries of prose describing races, classes
and realms that M7 and M9 are still changing.

*Side note worth checking, not asserting:* the race list in
[OBS-11](#obs-11--new-opens-the-birth-wizard) has no Ghoul, though it is in the four races
just added to the game. Ghoul may be a later-Zangband addition rather than a 2.4.0 race —
worth confirming against 2.7.5 before treating 2.4.0's roster as the reference for
completeness.

---

## OBS-13 — Birth options: four tabs, and the `$WILDERNESS` switch surfaces
*Screenshots: Options (Birth) / (Ironman) / (Quests) / (Wilderness), reached from `Options`
inside the birth wizard.*

**Birth:** point system · 'minimal' stats · maximize stats · preserve artifacts · terrain
streamers · ask for saving death.
**Ironman:** stores permanently closed · always unusually small levels · no climbing
up/recalling · permanent autoscummer · quest monsters get reinforcements · always empty
'arena' levels · always very unusual rooms · **Nightmare mode ("this isn't even remotely
fair!")**.
**Quests:** a number, 0–49, of random quests *in addition to the two required ones —*
**Oberon** and the **Serpent of Chaos**.
**Wilderness:** three radio buttons — multiple towns + quests + large wilderness / 'lite'
town + quests + small wilderness / 'vanilla' town without quests or wilderness, each with a
description pane.

**The Wilderness tab is the `$WILDERNESS` switch, exposed.** Plan §3.3 found
`?:[EQU $WILDERNESS NORMAL]` / `LITE` / `NONE` in `w_info.txt` and `t_info.txt`, selecting
between `t0000001.txt`, `t_lite.txt` and `t_basic.txt`. It is not a build-time setting — it
is **a birth option the player chooses**, and it is the single biggest shape-of-game switch
in Zangband. Worth knowing before Phase 2's M4 assumes one world scale.

### How these land against 4.2

| 2.4.0 option | 4.2 today |
|---|---|
| No climbing upwards / recalling | **Already there** — `birth_force_descend` + `birth_no_recall` |
| Preserve artifacts | Partly — `birth_no_artifacts`, `birth_lose_arts` |
| Point system | A birth *method* in 4.2's roller UI, not an option |
| Maximize stats, minimal stats, terrain streamers, ask for saving death, autoscummer | **Gone** — 4.2 made them unconditional or removed them |
| Nightmare mode | **Phase 2 M11**, already planned |
| Stores closed, small levels, arena levels, unusual rooms, quest reinforcements | Zangband-specific; no 4.2 equivalent |
| Random quest count | Phase 2 M6 |
| Wilderness scale | Phase 2 M4 — see above |

Agreed on skipping the quest count and the level-size knob for now: both are one integer and
one boolean whose value is in the game, not the front end, and neither blocks anything. Two
caveats on the "don't worry" list, though:

- **Nightmare mode is already in scope** as M11, so it is not a skip, just not ours yet.
- **The wilderness three-way is worth keeping.** "Vanilla town without quests and
  wilderness" is effectively *play it as Angband* — a meaningful compatibility and
  onboarding mode, and cheap while the data switch still exists. Phase 2's call, not
  Phase 3's, but it should be a deliberate one.

### The Phase 3 recommendation: generate the options UI, do not port it

`interface.html` is candid about the original: *"Options from the non-Tk version of the game
can be displayed, but these options have no effect."* A hand-built options UI drifted from
the game's real option set, and shipped with dead controls.

4.2 gives a better path. All **56** options live in one declarative table,
[list-options.h](../../src/list-options.h), each with a name, a description and a page, and
`OPT_PAGE_BIRTH` already separates birth options from the rest. So the bridge should expose
that table and **the Tcl should build the options windows from it** — including the birth
tabs, which fits the pre-game window from [OBS-01](#obs-01--a-startup-launcher-runs-before-the-game-exists).

That deletes most of `options.tcl` and `birth-options.tcl`, and makes a dead control
impossible: an option that exists is shown, an option that does not cannot be.

---

## OBS-14 — The autoroller, and what 4.2 replaced it with
*Screenshot: a modal "Autoroller" window over the term-rendered Character Info sheet —
per-stat percentage-of-target progress, "Hit any key to abort", and **Round: 952,800**.*

The autoroller re-rolls stats until they meet minimums the player set, which is what the
Birth tab's *"Specify 'minimal' stats"* option turns on. 952,800 rounds is that mechanism
doing what it always did: the requested minimums were near-unreachable, and it will spin
until interrupted. The percentages are progress toward each minimum, not stat values.

**Settled — no autoroller.** Confirmed by project owner: *"don't think we need the
autoroller."*

**This costs nothing, because 4.2 already removed it.** The only trace left is a comment in
[player-birth.c:775](../../src/player-birth.c#L775) written in the past tense — *"It was
feasible to get base 17 in 3 stats with the autoroller."* 4.2 offers two rollers and no
more: `BR_POINTBASED` and `BR_NORMAL`. So "minimal stats" being gone from 4.2
([OBS-13](#obs-13--birth-options-four-tabs-and-the-wilderness-switch-surfaces)) and the
autoroller being gone are the *same* removal, and the decision is just to leave it removed.

### The bigger find: 4.2 already has the wizard's state machine

Chasing the roller turned up [ui-birth.c:60](../../src/ui-birth.c#L60), and it materially
de-risks T8:

```c
enum birth_stage {
    BIRTH_BACK = -1, BIRTH_RESET = 0,
    BIRTH_QUICKSTART, BIRTH_RACE_CHOICE, BIRTH_CLASS_CHOICE,
    BIRTH_REALM_CHOICE, BIRTH_ROLLER_CHOICE, BIRTH_POINTBASED,
    BIRTH_ROLLER, BIRTH_NAME_CHOICE, BIRTH_HISTORY_CHOICE,
    BIRTH_FINAL_CONFIRM, BIRTH_COMPLETE
};
```

That is the wizard from [OBS-11](#obs-11--new-opens-the-birth-wizard) and
[OBS-12](#obs-12--the-class-step-and-where-the-old-flavour-would-sneak-back-in), stage for
stage — **including `BIRTH_BACK`**, so the `< Back` button's semantics already exist rather
than needing invention.

And two things that were expected to be Zangband-specific work are already present:

- **`BIRTH_REALM_CHOICE`** with `realm_slot` iteration, and
  [player.h:473](../../src/player.h#L473) sets `REALM_CHOICES 2` — which is exactly the
  **Realm 1 / Realm 2** pair on the Character Info sheet. The realm step has a home.
- **`BIRTH_HISTORY_CHOICE`**, matching the background paragraph on that sheet ("You are one
  of several children of a Yeoman…").

**What this does to T8.** `birth-tnb.c`'s 3,716 lines were partly implementing a flow 4.2
now provides. So T8 is not "build a character-creation wizard" — it is "give native views to
stages that already exist, and drive an existing state machine." That is a substantially
smaller and better-defined job than the line count suggested, though it does not change T8's
*position*: M7 and M9 still churn the content those views display.

---

## OBS-15 — The "classic" character sheet is not unfinished — it is a canvas, on purpose
*Screenshot: Character Info — the classic sheet, `'r' to reroll, 's' to restart, or ESC to
accept`, Life Rating 104/100, Spectre Chaos-Warrior, Realm 1 Chaos, Patron Chardros.*

The reasonable reading is that the author ran out of steam here: every other birth step is a
native wizard, and this one looks like a raw terminal dump with keyboard-only prompts. **It
is not.** The evidence is unambiguous:

- [`birth.tcl:2383`](../../archive/Tk/CommonTk-1.4/OmnibandTk/tk/birth.tcl) builds the
  prompt as a **canvas item** — `$canvas itemconfigure prompt -text $prompt` — and binds
  `r`, `s`, `p` and `Escape` as **Tk window bindings**, not term input. There is even a
  `'p' for prev` branch, gated on `birth info has_prev`, that the screenshot does not show.
- `charinfo-canvas.tcl` is **1,170 lines** of purpose-built canvas layout, plus
  `charflags-canvas.tcl` at 673 more for the equipment-flags table.
- Every label — `Adv Exp`, `Blows/Round`, `Saving Throw`, `Infravision` — comes from
  `msgs/player/en.msg`, so the sheet is **localised**, with a Japanese translation. A
  terminal dump cannot be translated.

So roughly 1,850 lines went into making this look like the classic sheet. That is a
deliberate reproduction of the most recognisable artefact in the game, in a widget that can
be clicked, coloured and annotated. It is the same decision as `interface.html`'s stated
philosophy, and it is why the Misc window's `EXP` and `AC` labels are clickable
([OBS-13](#obs-13--birth-options-four-tabs-and-the-wilderness-switch-surfaces) neighbours).

### This corrects the plan's architecture section

Plan §3.2 splits the UI into **two** tiers — term windows fed by `Term_*` hooks, and native
widgets fed by the 190-command bridge. There are **three**:

| Tier | Fed by | Examples | Looks like |
|---|---|---|---|
| **Term windows** | `Term_text` / `Term_pict` | the map, messages | a terminal |
| **Canvas text laid out as a terminal** | the bridge, drawn as canvas items | Character Info, character flags, the Misc panel | a terminal — but is addressable, localisable, clickable |
| **Native widgets** | the bridge | lists, toolbars, trees, dialogs | native |

The middle tier is the one that was invisible from screenshots, and it is where the original
did its most interesting work: **classic appearance without giving up interactivity.** A
player who reads the classic sheet fluently loses nothing, and gains hover, colour and
click.

That is a pattern worth adopting deliberately rather than rediscovering. It also reframes
§6 decision 7 ("term or native for full-screen displays?"): the real answer is often
*neither* — canvas, styled classic.

*Aside:* `Patron: Chardros` here, and `Mabelode` on the earlier plate — both Moorcock, both
already replaced in [patron.txt](../../lib/gamedata/patron.txt). See
[OBS-12](#obs-12--the-class-step-and-where-the-old-flavour-would-sneak-back-in) for why the
prose, not the data, is the leak to guard.

---

## OBS-16 — Two separate mechanisms hold the windows together, and one may be a Wine artefact
*Reported, then corrected: "the windows do move independently, but when you move the main
window all the other smaller windows move with it."*

The correction matters, because it separates two mechanisms I had run together.

### Mechanism 1 — a central geometry authority, for *initial* placement

[`tk/library/window-manager.tcl`](../../archive/Tk/CommonTk-1.4/OmnibandTk/tk/library/window-manager.tcl),
317 lines. Every toplevel calls `NSWindowManager::RegisterWindow` handing over a geometry
command, a setup command and a display command, and the manager owns `RequestGeometry`,
`Setup`, `Arrange` and `ArrangeAll`. Defaults are computed against `winfo screenwidth` with
explicit branches at **800 pixels** (`main-window.tcl:300`, `misc-window.tcl:343`) — the two
screen-size bands `interface.html` warns about. Positions persist **as a set**, through
`Save Window Positions` on the Window menu.

This decides where windows *start* and where a saved layout puts them. It does not stop the
user dragging one afterwards — which is the part of my earlier note that was wrong.

### Mechanism 2 — every satellite is a Tk transient of Main

`wm transient $win [Window main]` appears in **~40 scripts**, including all six of the
visible satellites — `misc-window.tcl` alone does it five times (Misc, Micro Map,
Progress…), plus `choice-window.tcl`, `recall.tcl`, `message-window.tcl`. So the whole fleet
is declared subordinate to Main.

**But what `wm transient` *does* is platform-specific, and that is the catch:**

| Platform | Behaviour |
|---|---|
| X11 | Sets `WM_TRANSIENT_FOR`. Most WMs keep it above the parent and iconify with it. Moving with the parent is **not** standard |
| Win32 | Tk maps it to an *owned* window: stays above the owner, minimises with it, **does not move with it** |
| macOS / Aqua | Tk has used `NSWindow` child relationships here — and macOS child windows **do** move with the parent |

The reference build is the **Windows** binary under CrossOver, so Wine is implementing Win32
owned-window semantics on top of the macOS window server. Satellites following Main is very
likely **Wine mapping owned windows onto macOS child windows** — behaviour that would not
have happened on Windows 98, where they would have stayed on top and minimised together but
held their position.

**So treat "they move together" as unverified for the original.** This is the standing
caveat at the top of this log doing exactly its job. It would be settled by running the
*same* scripts under a native Aqua `wish`, which is a Phase 3 activity anyway.

### It does not change the recommendation — it improves the argument for it

Forty `wm transient` calls plus a central geometry authority plus 800-pixel layout bands is
not a person who wanted six independent windows. It is a person who wanted **one composed
window**, and had to build it out of toplevels because Tk 8.0 had no good pane widget and
the screen was 800×600. Stay-on-top, minimise-together and computed placement are all
approximations of panes.

The intent was a single composed window. Tk 9 can express that intent directly. So:

| | What it is | Cost |
|---|---|---|
| **A. One window, panes** | Ttk `panedwindow`/grid with user-draggable sashes | Native, full-screen-friendly, one thing to manage. Loses parking Messages on a second monitor |
| **B. Truly independent toplevels** | Drop the manager; users place windows, we only remember where | Multi-monitor friendly. Ugly first launch, lots of chrome, and the transient semantics have to go |
| **C. Reproduce transients + geometry manager** | Faithful to 2001 | Reproduces a workaround for constraints that no longer exist — *and* whose observed behaviour we cannot even confirm is original |

**Recommend A**, with detachable panes as a post-T7 option. Keep the *idea* of a central
layout authority: named, switchable layouts beat one `Save Window Positions` menu item.

→ Plan decision 1, landing in T1.

---

## OBS-17 — The menus show their accelerators, and that is the whole design
*Screenshot: Inven → Magic, with `Activate A`, `Aim Wand a`, `Drink Potion q`,
`Read Scroll r`, `Use Staff u`, `Zap Rod z`, `Browse b`, `Study G`.*

*"All the keyboard commands are available through the menus, which is the key strength of
this version as you don't need to remember the keyboard commands."*

Agreed, with one refinement that is the crux of it: **every item shows its key beside the
label.** The menus do not replace the keyboard — they *teach* it. A novice uses the menu,
sees `q` next to Drink Potion every time, and is typing `q` within an hour. That is why this
is accessible without being dumbed down, and it is the difference between a good roguelike
menu and a bad one.

Make that a rule for T7: **no menu item without its accelerator displayed.**

### 4.2 already holds the entire menu bar as data

[ui-game.c:388](../../src/ui-game.c#L388) is a named group table —

```
{ "Items",           cmd_item,        … }
{ "Action commands", cmd_action,      … }
{ "Manage items",    cmd_item_manage, … }
{ "Information",     cmd_info,        … }
{ "Utility",         cmd_util,        … }
{ "Hidden",          cmd_hidden,      … }
```

— and each group is an array of `cmd_info`, every entry carrying what a menu needs:

```c
{ "Aim a wand",    {'a', 'z'}, CMD_USE_WAND, NULL, NULL, … },
{ "Read a scroll", { 'r' },    CMD_READ_SCROLL, NULL, player_can_read_prereq, … },
{ "Fire your missile weapon", { 'f', 't' }, CMD_FIRE, NULL, player_can_fire_prereq, … },
```

A **human-readable label**, the **keys** (primary plus alternate), the command code, and — the
part worth noticing — a **prereq function**.

**So generate the menu bar from that table, exactly as with the options windows
([OBS-13](#obs-13--birth-options-four-tabs-and-the-wilderness-switch-surfaces)).** Four
things fall out, three of them free:

1. **Labels** come from the table, so a menu entry cannot name a command that does not exist.
2. **Accelerators** come from `cmd_lookup_key()` ([ui-game.h:63](../../src/ui-game.h#L63)),
   which resolves the *current* binding — so the displayed key follows the player's own
   keymap instead of being a hard-coded string that lies after they rebind anything.
3. **Greying out comes free, and the original did not have it.** `player_can_read_prereq`,
   `player_can_fire_prereq`, `player_can_refuel_prereq` already exist. Read Scroll can be
   disabled while blind, Fire with no launcher, Fuel with nothing to fuel. In the screenshot
   every Magic entry is enabled regardless of whether the character can use it — a menu that
   offers impossible actions is exactly what a beginner cannot evaluate. This is a place the
   port can be **better than the original** at no cost.
4. **Nothing drifts.** `interface.html` admits the original's options UI shipped dead
   controls; hand-authored menus rot the same way.

### The one part that is real design work

4.2's grouping is *its* taxonomy — Items / Action commands / Manage items / Information /
Utility. ZangbandTk's menu bar is a different, player-facing one: **File / Inven / Book /
Action / Other / Tool / Window / Help**. And the screenshot shows why the difference is not
cosmetic: `Inven → Magic` mixes item-use commands (Activate, Aim Wand, Drink Potion, Read
Scroll, Use Staff, Zap Rod) with *spell* commands (Browse, Study). That is a grouping by
what the player is trying to do, not by what the code calls it.

So T7 needs a **declarative menu map** — menu → submenu → ordered list of command codes —
sitting on top of 4.2's table, plus entries for Zangband-specific commands that Phase 2
adds. The leaf data is all there; the arrangement is ours, and it is worth arranging by
intent rather than inheriting either taxonomy wholesale.

→ Folds into plan T3 (the bridge must expose the command table, including
`cmd_lookup_key()` and the prereqs) and T7 (the menu map, and the accelerator rule).

---

## OBS-18 — What `Note` does, and why `cmd_hidden` is a trap for the menu generator
*Screenshot: Action → Alter ▸ / Looking ▸ / Movement ▸ / Resting ▸ / Searching ▸ · Note `:`
· Repeat `n` · Target `*` · Pets `p` · Use Power `U`. Messages shows `Note: dddd`.*

### What Note does

`:` prompts `Note:` and records free text against the character — a player journal for
"came back for the vault on 1250ft" or "left the Broad Sword in the Home".

**Zangband 2.4.0** wrote it to a sidecar file: `notes.c` (176 lines) builds
`lib/save/<charname>.txt` and appends there.

**4.2 already has the command, and implements it better.** Same key, at
[ui-game.c:244](../../src/ui-game.c#L244) — `{ "Take notes", { ':' }, CMD_NULL,
do_cmd_note, … }`. And [cmd-misc.c:88](../../src/cmd-misc.c#L88) reads a line and calls
`history_add(player, note, HIST_USER_INPUT)`, so the note lands in the character's
**history** — the same timeline that records level gains, artifact finds and death. It is in
the savefile and in the character dump, interleaved with the automatic entries, instead of a
loose text file beside it.

Nothing to port. `notes.c` joins the drop list.

### The trap: `cmd_hidden` is not "hide these from the player"

Tracing this menu produced the useful finding. Almost the whole Action menu lives in 4.2's
`cmd_hidden[]`:

| ZangbandTk Action entry | 4.2 command | 4.2 group |
|---|---|---|
| Note `:` | Take notes | **Hidden** |
| Repeat `n` | Repeat previous command | **Hidden** |
| Alter ▸ | Alter a grid (`+`) | **Hidden** |
| Movement ▸ | Walk · Start running · Stand still · Start exploring | **Hidden** |
| Searching ▸ | Steal from a monster (`s`), Do autopickup | **Hidden** |
| *(the `C` button in the status bar)* | Center map | **Hidden** |
| Target `*` | Target monster or location | `cmd_action` |
| Resting ▸ | Rest for a while (`R`) | `cmd_action` |
| Looking ▸ | Look around (`l`) | `cmd_action` |

`cmd_hidden` means **"4.2's own keyboard-driven menu does not list these"** — mostly because
they are the implicit action of a direction key, or a utility. It does **not** mean the
player should not see them. A GUI wants every one of them: `Movement ▸` as a submenu is
exactly how a mouse-first player discovers that running and walking are different commands.

**So [OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)'s
menu map must be able to promote from `cmd_hidden`, and must not mirror 4.2's groups.** A
generator that iterated the visible groups would silently drop Note, Repeat, Alter,
Movement, Searching and Center map — six of the ten Action entries.

### Two key collisions, found early

Zangband and 4.2 disagree about two keys, and both matter because Phase 2 is adding the
Zangband side:

| Key | ZangbandTk 2.4.0 | Angband 4.2 |
|---|---|---|
| **`p`** | **Pets** — the pet command menu | **Start exploring** (`cmd_hidden`) |
| **`U`** | **Use Power** — racial and mutation powers | **Use an item** (`cmd_item`) |

Neither is resolvable by the front end alone: `p` collides with pets, which M10 adds
(PLR-22), and `U` collides with racial powers, which M7 and M8 add. Both need a game-side
decision about which command owns the key, and **the menu is where a wrong answer becomes
visible** — an accelerator that displays one thing and does another is worse than no
accelerator at all ([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)).

Raise these with M7/M8/M10 rather than deciding them in T7.

---

## OBS-19 — Inven → Use, and one semantic trap
*Screenshot: Destroy `k` · Drop `d` · Pick Up `g` · Take Off `t` · Wear/Wield `w` ‖ Eat Food
`E` · Fire Missle `f` · Fuel Light `F` · Jam Spike `j` · Throw `v`.*

Eight of the ten land on 4.2 unchanged — `d` `g` `t` `w` `E` `f` `F` `v` are all
`cmd_item` entries with the same keys. Two do not, and one of them is a trap.

### `Destroy k` is **not** a rename — the semantics changed

4.2 has the key, at [ui-game.c:167](../../src/ui-game.c#L167):

```c
{ "Ignore an item", { 'k', KTRL('D') }, CMD_IGNORE, textui_cmd_ignore, … }
```

Same key, different act. Zangband's **Destroy** removed the item from the game. 4.2's
**Ignore** is squelch: the item stays on the floor and is hidden from display and pickup.
Labelling that menu entry "Destroy" would be actively misleading — a player would use it
expecting the item gone.

So the menu map cannot be keyed on the old *labels*; it has to be keyed on 4.2's command
codes, and take 4.2's label. This is the one case so far where inheriting the original's
wording would ship a lie.

*Archive footnote:* `squelch_patch_source_atk_ztk-1_0.zip` and its runtime companion are a
2002 third-party patch that **added** squelch to AngbandTk and ZAngbandTk. 4.2 has it
natively, so that patch is obsolete — worth knowing before someone tries to port it.

### `Jam Spike j` is gone

No `CMD_SPIKE`, no spike command, and no iron spikes as an object kind — the only match in
`object.txt` is flavour text on a crown. 4.2 removed door-spiking entirely. The entry has no
destination and should not be recreated unless Phase 2 deliberately brings spikes back.

*Also:* the original reads **"Fire Missle"**. A typo, in the shipped product, for twenty-five
years. Do not reproduce it.

### The pattern worth naming

This is the third menu to pay off, and they are all paying off the same way: **the original's
menu bar is a differential audit of Zangband-versus-4.2 command coverage.** Each menu says,
per command, whether 4.2 kept it, moved it, renamed it, silently changed its meaning, or
dropped it:

| Outcome | Examples so far |
|---|---|
| Kept, same key | `d` `g` `t` `w` `E` `f` `F` `v` `q` `r` `u` `a` `z` `A` `b` `G` `:` `n` `*` `R` `l` |
| Kept but in `cmd_hidden`, so a naive generator drops it | Note, Repeat, Alter, Movement ▸, Searching ▸, Center map |
| Same key, **different meaning** | `k` Destroy → Ignore |
| Key collision with a Zangband feature | `p` Pets vs Start exploring · `U` Use Power vs Use an item |
| Removed from 4.2 | `j` Jam Spike |

That table *is* the T7 menu map's input, and it costs nothing to collect — one screenshot
per menu.

**So the highest-yield thing to capture next is the rest of the menu bar:** **File**,
**Book**, **Other**, **Tool**, **Window**, **Help**. `Other` and `Tool` are the interesting
ones, since that is where the Zangband-specific commands and the front end's own tools will
be.

---

## OBS-20 — The `Other` menu: three categories, one delightful loop, one more collision
*Screenshot: Character Info `C` · Feeling `Ctrl+F` · File Character · High Scores ·
Knowledge `~` · Message History `Ctrl+P` · Photo ▸ · Preferences ▸ · Quest Status `Ctrl+Q` ·
Time Of Day `Ctrl+T` ‖ Use Music · Use Sound.*

As predicted, `Other` is the interesting menu — it mixes three different kinds of thing that
the menu map has to treat differently:

| | Entry | Destination |
|---|---|---|
| **4.2 already has it, same key** | Character Info `C` | `{ "Character description", { 'C' }, do_cmd_change_name }` |
| | Feeling `Ctrl+F` | `{ "Repeat level feeling", { KTRL('F') }, do_cmd_feeling }` |
| | Knowledge `~` | `{ "Check knowledge", { '~' }, textui_browse_knowledge }` |
| | Message History `Ctrl+P` | `{ "Show previous messages", { KTRL('P') }, do_cmd_messages }` |
| | File Character, High Scores | 4.2 has both, via its own utility commands |
| **Front-end only — no game command at all** | Photo ▸ | see below |
| | Preferences ▸ | Sound / Color / Assign / Font |
| | Use Music, Use Sound | collapse to one preference ([OBS-04](#obs-04--sound-offers-three-backends-all-of-them-windows)) |
| **Zangband-specific game commands** | Quest Status `Ctrl+Q` | Phase 2 **M6** — no 4.2 equivalent |
| | Time Of Day `Ctrl+T` | Phase 2 **M4** (day/night in the wilderness) — no 4.2 equivalent |

**A third key collision.** `Ctrl+T` is Time Of Day here, but 4.2 binds it as an *alternate*
for tunnelling: `{ "Dig a tunnel", { 'T', KTRL('T') }, CMD_TUNNEL }`. Less severe than `p`
and `U` — it is 4.2's secondary binding, so dropping the alternate is a cheap fix — but it
belongs on the same list.

### `Photo` is the loop-closer

Photo is not a screenshot tool. `photo.tcl` (377 lines) plus `photo-window.tcl` (536)
capture a **map region together with its cave data**: `NSPhoto::PointToCave`,
`ExamineWidget` and `Motion` mean you can point at a saved photo afterwards and it still
tells you what is on each grid. It is an inspectable snapshot, saved as image plus text.

And `NSPhoto::WritePhotoText` emits exactly this:

```
gwidth: $gwidth gheight: $gheight
xmin: $xmin xmax: $xmax ymin: $ymin ymax: $ymax
$y $x $what $idx "$examine"
```

Which is, byte for byte, the format of `preview-adam24.txt` from
[OBS-02](#obs-02--setup-chooses-the-tileset-before-the-game-starts-with-a-live-preview):

```
gwidth: 24 gheight: 24
xmin: 40 xmax: 57 ymin: 45 ymax: 56
47 52 object 178 "You see a Scroll titled "banmur dan evs""
50 50 monster 46 "You see a Novice mage (sleeping)"
```

**The tileset previews are photos.** The author played the game, found a scene that showed
off terrain, a monster and an object, took a photo of it, and shipped the photo text as the
preview spec. Which means the settled live-render decision and the Photo feature are *the
same feature*: implement Photo and the preview authoring tool comes free, and implement the
preview renderer and Photo is most of the way there.

It also validates that decision from an unexpected direction — the author's own preview
pipeline went through the real renderer, because it was literally a capture of the real
renderer.

*One design note for our version:* photo text stores monsters and objects by **index**
(`object 178`, `monster 46`), which is the same brittleness as everything else keyed to
2.4.0 numbering. Store by name.

---

## OBS-21 — The Character window has five tabs, and two of them are M8's destination
*Screenshot: Character — tabs **Info · Flags · Mutations · Virtues · Notes**.*

`character-window.tcl:41` confirms it in code — `HookMutations`, `HookVirtues` — alongside
the Info sheet ([OBS-15](#obs-15--the-classic-character-sheet-is-not-unfinished--it-is-a-canvas-on-purpose))
and the Flags table.

This answers a question the plan had not asked: **where do Zangband's player systems get
displayed?** Phase 2's M8 produces 96 mutations and eight virtues, and until now the plan
said what to implement but not where any of it surfaces. The answer is here, and it was
designed in 2001:

| Tab | Content | Phase 2 source |
|---|---|---|
| Info | the classic sheet, canvas-rendered | — |
| Flags | resistances per equipment slot | — |
| **Mutations** | the character's current mutations | **M8** |
| **Virtues** | the eight virtues and their standings | **M8** |
| **Notes** | the player journal | [OBS-18](#obs-18--what-note-does-and-why-cmd_hidden-is-a-trap-for-the-menu-generator) |

Two consequences:

1. **M8 has a UI destination**, so "mutations and virtues" is not just game state with no
   way to read it. Worth telling M8 that the display exists and is a tab, not a screen.
2. **The Notes tab is where 4.2's history should surface.** 4.2 routes `:` into
   `history_add()`, and that history is richer than Zangband's flat text file — level gains,
   artifact finds and player notes interleaved on one timeline. So this tab becomes a
   *character history* view with the player's own notes in it, which is strictly more useful
   than the original's tab.

---

## OBS-22 — The Character window's four other tabs: three work, one is switched off
*Screenshots: Flags (labels but no marks) · Mutations (empty) · Virtues (eight "You are
neutral to…") · Notes (empty despite notes having been entered).*

Four questions, four different answers — and only one is a defect.

### Flags — working, and here is the test

`charflags-canvas.tcl:38` sets `Priv(slots)` to `INVEN_WIELD, INVEN_BOW, INVEN_LEFT,
INVEN_RIGHT, INVEN_NECK, INVEN_LITE, INVEN_BODY, INVEN_OUTER, INVEN_ARM, INVEN_HEAD,
INVEN_HANDS, …`. The **columns are equipment**; the rows are the flags down the left.
`interface.html` is precise about it: *"one icon for each equipment **item** and one icon for
the character"* — items, not slots. So an unequipped character gets exactly **one** column,
which is the lone icon at the top right of the screenshot.

And this character has nothing equipped: Armor reads `[0,+1]`, while the Metal Scale Mail
`[13,+0]` and Broad Sword are still sitting in the Choice list. A level-1 Human
Chaos-Warrior also has no intrinsic flags, so the one column it does draw is legitimately
blank.

**Confirmed.** Equipping produced three columns — Broad Sword, Metal Scale Mail, character
— and five marks, all in the *character* column: Resist Sanity Blast, Slow Digestion, See
Invisible, Hold Life, Absorb Nether. Those are the **Spectre** intrinsics, matching the
birth text word for word ("As undead, they have a firm hold on their life force, see
invisible … They also resist nether"). The two item columns are empty because a plain Broad
Sword and Metal Scale Mail carry no flags. The tab is correct.

### …but it reads badly, and that is our problem to fix

Noted at the time as "does not look nice, but that is a different issue". It is a different
issue, and it is **ours**, so it belongs here. Four distinct faults, all fixable:

1. **The noise outnumbers the signal.** Flags the character lacks are drawn as dim labels,
   so most of the list is grey text and five white dots have to be hunted for.
2. **Label and mark are a screen apart.** Three columns spread across ~900px with the marks
   at the right edge; matching a dot to its row takes a straightedge.
3. **No banding or rules** to carry the eye across that gap.
4. **No grouping.** Forty-odd flags scroll as one flat list, with resistances, sustains and
   abilities interleaved.

Fixes are well understood: filter to relevant rows by default with a "show all" toggle,
group by kind, band alternate rows, and keep columns adjacent to the labels rather than
spread to the window's width. A Ttk treeview does most of it natively.

**And it makes a principle explicit, which is the real value of the observation.** This is
the second time faithful reproduction would have reproduced a *usability* problem rather
than a feature — the first being the coupled window group in
[OBS-16](#obs-16--two-separate-mechanisms-hold-the-windows-together-and-one-may-be-a-wine-artefact).
So: **copy the affordances, not the layouts.** The paper doll's placeholder slots
([OBS-25](#obs-25--the-items-window-is-a-paper-doll-and-it-is-the-best-thing-in-the-front-end)),
the accelerators beside menu items
([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)), the
linked cursor, the live character in the score table — those are the ideas worth having. The
1999 pixel arrangements around them are not.

### Mutations — the assumption was right

`HookMutations`'s display branch is `foreach desc [angband player mutations]`. No
mutations, no rows. A level-1 character has none; Zangband hands them out through patron
gifts, chaos effects and certain potions. Empty is correct.

### Virtues — and the repo is ahead of the plan here

Eight virtues, all neutral, always rendered — Chance, Individualism, Valour, Sacrifice,
Justice, Honour, Temperance, Compassion.

**The eight are per-character, drawn from a larger pool**, which is why
[decisions.md](decisions.md) names *Harmony* as a patron virtue and it does not appear here:
a Human Chaos-Warrior simply drew a different eight.

And the pool is already implemented in this repo. [src/list-virtues.h](../../src/list-virtues.h)
— *"The eighteen virtues a character may be measured against"* — holds exactly the eighteen
Zangband has: Compassion, Honour, Justice, Sacrifice, Knowledge, Faith, Enlighten, Enchant,
Chance, Nature, Harmony, Vitality, Unlife, Patience, Temperance, Diligence, Valour,
Individualism. [player.h](../../src/player.h) carries `MAX_RACE_VIRTUES`,
`MAX_CLASS_VIRTUES`, `MAX_REALM_VIRTUES` and `MAX_PLAYER_VIRTUES`, with
[player-virtue.h](../../src/player-virtue.h) tagged **PLR-18 to PLR-21**.

So the Virtues tab has a data source *today*. It is the cheapest tab to build and it can be
built before M8 finishes, which makes it a good early proof that the Character window's
frame works.

### Notes — not a defect. The feature is off by default

`cmd4.c:2641`:

```c
if (take_notes) { add_note(buf, ' '); }
else            { msg_format("Note: %s", buf); }
```

`take_notes` **defaults to FALSE** (`tables.c:6212`, *"Allow notes to be appended to a
file"*). With it off, `:` writes the note to the **message log only** — which is exactly
what was seen: `Note: dddd` in Messages and the Main window's top line, and nothing in the
tab. Turn the option on and the tab fills. There is also `auto_notes` (`tables.c:6215`,
*"Automatically note important events"*), also off by default.

**And this is a Phase 3 lesson, not just an answer.** The UI showed an empty tab for a
switched-off feature and gave no hint why. That is the same failure class as offering menu
items the character cannot use
([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)): the
interface knows something the player needs to know and does not say it. Our version should
render *"Note-taking is off — enable it in Options"*, with the option one click away. Empty
states that explain themselves are most of what "accessible" means, and they are nearly
free.

---

## OBS-23 — The Hall of Fame is a native window with two features worth keeping
*Screenshot: "ZAngbandTk Hall of Fame", `Score` and `Sort` menus, three entries, the live
character boxed in cyan, "No record available." in a status bar.*

Two things the term-rendered score list cannot do, both cheap:

1. **`Sort`** — a sort menu. The classic list is fixed order.
2. **The live character appears inline**, boxed and highlighted, reading *"Killed by nobody
   (yet!)"* and `Date TODAY`. So the player sees where they currently stand *among* the dead
   rather than having to imagine it. That is a genuinely nice touch and costs nothing.

The status bar reading **"No record available."** implies a third feature: a per-entry
detail pane, presumably the character dump for a dead entry. Worth confirming by selecting
one of the two dead characters.

**4.2 has everything needed.** [src/score.h](../../src/score.h) defines `struct high_score`
with `what, pts, gold, turns, day, who, uid, p_r, p_c, cur_lev, cur_dun, max_lev, max_dun,
how`, and `highscore_read()` returns them as an array. Every column in that screenshot maps
to a field, and sorting is then trivially a Tcl-side operation on a list — no game-side work
at all.

*Aside:* the two "Underdogs" entries are dated 04/19/02 and came with the archive —
`lib/apex/z_scores.raw` ships pre-populated. Someone's 2002 Chaos-Warriors, killed by an
insect swarm and a mean looking mercenary, are still in the file.

---

## OBS-24 — The Knowledge window: six tabs, three of them Zangband's
*Screenshots: Knowledge — tabs **Artifacts · Monsters · Objects · Pets · Quests · Home**.
Two panes, groups left and members right, each name carrying its **tile icon**, with
"1 in group" in a status bar. The Quests tab lists "Oberon: Kill Oberon, King of Amber
(incomplete)" and "Serpent of Chaos: Kill The Serpent of Chaos (incomplete)".*

`interface.html` describes the two-pane behaviour — click a group, get its members, click a
member, get the recall — plus search by name. What it does not mention is the tab set, and
half of it is Zangband's:

| Tab | 4.2 equivalent | Notes |
|---|---|---|
| Artifacts | yes | 4.2 also has ego items, runes, features and traps that this lacks |
| Monsters | yes | 4.2 additionally tracks kill counts |
| Objects | yes | |
| **Pets** | **none** | Phase 2 **M10**. A knowledge view of your own pets |
| **Quests** | **none** | Phase 2 **M6** |
| **Home** | **none** | Home contents — matters once M5 gives several towns each with a home |

Two details worth copying: **every row carries its tile**, which the term list cannot do and
which makes a monster list scannable at a glance; and the **"n in group" count** in the
status bar. Both are free once the tile engine exists.

Two details worth improving: 4.2 knows about **ego items, runes, features and traps**, none
of which the original had a tab for, and it tracks **monster kill counts**. The tab set
should be 4.2's *plus* Pets, Quests and Home — not the original's.

**The Quests tab is on target for DEC-01.** *"Kill Oberon, King of Amber"* — the mandatory
quests were Amber's from the beginning, and they are the one part of Zangband's endgame that
needs no correction. Worth noting for M6, which otherwise inherits a lot that does.

---

## OBS-25 — The Items window is a paper doll, and it is the best thing in the front end
*Screenshot: Items — a body silhouette in a red cloak with twelve slot boxes placed
anatomically and joined to the figure by leader lines, colour-coded borders, an inventory
icon grid on the right, an `Inscription:` field in the toolbar, and weight readouts
`57.0/72.0/120.0 lb` and `5 items · 57.0 lb`.*

This is the single most graphical thing in the whole front end, it appears in **no**
document, and it is invisible from every other screenshot. `interface.html` describes the
Inventory Window as a *list*. There are two implementations — `inventory.tcl` (1,728 lines,
the list) and **`inventory2.tcl` (2,631 lines, this)**.

It is also entirely **data-driven**, which makes it portable. `inventory2.tcl:330`:

```
INVEN_WIELD 111 158 {119 0 0} {204 0 0}
INVEN_BOW    72  23 {119 0 0} {204 0 0}
INVEN_LEFT  160 171 {51 102 102} {102 204 204}
INVEN_NECK   51 148 {51 102 102} {102 204 204}
INVEN_LITE   26  31 {102 51 0}   {255 153 0}
INVEN_BODY   95  89 {0 102 0}    {51 204 0}
…
```

Per slot: an **x,y position on the body image** and a **dark/light border colour pair**. A
second table gives the leader-line endpoints. And the colours are not decoration — they are
four **slot categories**:

| Colour | Slots | Category |
|---|---|---|
| Red | WIELD, BOW | weapons |
| Teal | LEFT, RIGHT, NECK | jewellery |
| Brown/orange | LITE | light source |
| Green | BODY, OUTER, ARM, HEAD, HANDS, FEET | armour |

**Why it matters more than it looks.** Empty slots are drawn as placeholders, so the screen
answers a question a lettered list cannot: *what could I be wearing that I am not?* For a
new player that is the single most useful thing an equipment screen can say, and it is why
this belongs in T7 rather than being filed as decoration.

### 4.2 maps onto it cleanly, and is more flexible

[src/list-equip-slots.h](../../src/list-equip-slots.h) is the slot taxonomy — `WEAPON`,
`BOW`, `RING`, `AMULET`, `LIGHT`, `BODY_ARMOR`, `CLOAK`, `SHIELD`, `HAT`, `GLOVES`, `BOOTS`
— each with two flags and display strings (`"Wielding"`, `"Shooting"`, `"On %s"`,
`"Around %s"`, `"Light source"`). Every original slot maps 1:1, with `INVEN_LEFT`/`RIGHT`
both becoming `RING`:

`WIELD→WEAPON · BOW→BOW · LEFT,RIGHT→RING · NECK→AMULET · LITE→LIGHT · BODY→BODY_ARMOR ·
OUTER→CLOAK · ARM→SHIELD · HEAD→HAT · HANDS→GLOVES · FEET→BOOTS`

The categorisation is derivable from the table's flags and display strings, so the four
colour groups need no hand-maintained list.

**One real difference to design around: 4.2's slot set is not fixed.**
[player.h:225](../../src/player.h#L225) has `struct player_body { name, count, slots }`, and
[lib/gamedata/body.txt](../../lib/gamedata/body.txt) defines bodies by name — one
(`Humanoid`) today, but the mechanism exists for a race with a different slot set.

So the paper doll must be built from **`player->body`** at runtime, not from a hard-coded
list of twelve. That in turn means the coordinate table cannot be a flat list either: it has
to be *per body type*, keyed by slot name. Given Phase 2 adds Zangband's races — and
Skeletons, Spectres and Zombies are exactly the sort of thing that might not wear boots —
this is worth getting right first time rather than retrofitting.

**Assets needed:** a body silhouette per body type. The archive's is drawn into the Tcl
rather than shipped as a GIF, so this is the one piece of art that has to be made rather
than salvaged. The coordinates, however, are all in `inventory2.tcl:330-346` and transfer
directly for `Humanoid`.

*Also in that toolbar:* an inline **`Inscription:`** field, so inscribing is a text box
rather than the `{` command plus a prompt. Small, and strictly better.

---

## OBS-26 — Photo confirmed, `Open` crashes, and Tk 9 absorbs all three dead extensions
*Screenshots: Other → Photo ▸ New / Open. "Photo #1 - ZAngbandTk" holds a snapshot of the
dungeon view. `Open` crashes the application.*

`New` works and confirms [OBS-20](#obs-20--the-other-menu-three-categories-one-delightful-loop-and-one-more-collision):
a photo is a captured map view in its own window, numbered (`Photo #1`), with its own
`Photo` menu.

### Why `Open` probably crashes — and it is probably not the app

`NSPhotoWindow::Open` (`photo-window.tcl:362`) does two things that could fail, and only one
of them is guarded:

1. `tk_getOpenFile -filetypes {GIF JPEG PNG} -initialdir [PathUser]` — **unguarded**.
2. `$image configure -file $filePath` — wrapped in `catch`, so a format Tk cannot read
   produces a "Photo Error" dialog, not a crash. (And it *would* fail on JPEG and PNG: Tk
   8.3's photo image reads only GIF and PPM/PGM, and the `Img` package that adds the rest is
   **not shipped** — `lib/` contains Dbwin, TclZip, TkHtml, UpgradeTool, sound and
   tk_chooseDirectory, no Img. The save path checks for Img and says so politely; the open
   path does not check at all.)

That leaves the file dialog. On Windows, Tk's `tk_getOpenFile` calls straight into
`comdlg32`'s `GetOpenFileName` — **outside Tcl entirely, so no `catch` can save it** — and
comdlg32 is a well-known soft spot in Wine. The asymmetry fits: `New` opens no dialog and
works; `Open` opens one immediately and dies.

**A clean test that separates the two hypotheses:** try any *other* file dialog —
`Other → File Character`, the `Browse…` button in the launcher's Open Game, or a photo
save. If they all crash, it is Wine's comdlg32 and not ZangbandTk.

**That would matter beyond this one menu item**, because it would mean the reference build
cannot be used to observe *any* file-dialog flow: Open Game's Browse, File Character, photo
save, and the pref-file load/dump in the Macros window. Worth knowing which of the capture
list is simply unreachable.

### The porting payoff: Tk 9 replaces every one of the original's binary extensions

Three third-party Tcl extensions the original depended on are all dead — and Tk 9 core
makes all three unnecessary:

| Extension | What it did | Replacement |
|---|---|---|
| **`Img`** | JPEG/PNG reading and writing for photos | **Tk 9 reads PNG natively** — `tkImgPNG.c` is compiled into the build verified in plan §2.1 |
| **`TkHtml`** (D. R. Hipp, 1997–98, LGPL) | rendered the help system | **Sphinx**, per DEC-17 and §6 decision 7 |
| **`TclZip`** | packed saved photos into a zip (`$zipCmd add $photoFile photo.gif`) | **Tcl 9 zipfs**, confirmed present in the built interpreter (`::tcl::zipfs::mount` exists) |

So the answer to "what do we do about the dead extensions" is: nothing. Delete all three
dependencies. That also disposes of `lib/TclZip` and `lib/TkHtml`, already on the drop list
in plan §4, and removes the last reason to want `Img`.

---

## OBS-27 — The ten Preferences, and what survives of them
*Screenshot: Other → Preferences ▸ Alternate · Assign · Color · Font · Keymap · Macros `@` ·
Music · Options `=` · Sound · Sprite.*

The full preference surface, and it collapses considerably:

| Preference | What it configures | Our version |
|---|---|---|
| **Options** `=` | the game's options | **Generated** from [list-options.h](../../src/list-options.h)'s 56 entries ([OBS-13](#obs-13--birth-options-four-tabs-and-the-wilderness-switch-surfaces)) |
| **Keymap** | the visual keyboard, keymaps per key | **Keep.** 4.2 has keymaps; the keyboard picture is a genuinely good affordance |
| **Macros** `@` | macro triggers and actions | **Keep**, but see below — the original's macros used X11 keysym names and it warned they broke compatibility |
| **Color** | the game's colour table | **Keep.** 4.2 has `angband_color_table`; `lib/customize/` has no colour prf, so this is a real feature the term build lacks |
| **Font** | per-window fonts | **Keep** — per-pane under the one-window model (decision 1) |
| **Assign** | which tile (and sound) each entity uses | **Tile half only.** The sound half is settled out ([OBS-13](#obs-13--birth-options-four-tabs-and-the-wilderness-switch-surfaces) neighbours / the per-monster decision) |
| **Alternate** | per-terrain tile variants (`*-alternate`, 224 lines for dg32) | **Open** — depends on §6 decision 4 |
| **Sprite** | multi-frame **tile animation** (`*-sprite`, 74 lines for dg32) | **Gap — see below** |
| **Music** | BASS on/off | **Drop** ([OBS-03](#obs-03--music-is-bass-and-only-bass)) |
| **Sound** | backend choice + volume | **Collapse** to volume and on/off ([OBS-04](#obs-04--sound-offers-three-backends-all-of-them-windows)) |

Ten down to about six, two of which are generated rather than written.

### The Sprite gap

`Sprite` drove per-entity **animated tiles** — the `*-sprite` config files, and the
`sprite` icon type that appears in the `.vlt` header row alongside `alternate` and
`flavor`. So a fire elemental could flicker and water could ripple.

4.2 has no equivalent. Its only animation-adjacent mechanism is the neon set's hue shimmer:
`cycle:10:4:9` in [lib/tiles/list.txt](../../lib/tiles/list.txt), described in
[grafmode.h](../../src/grafmode.h#L38) as `cycleHues`/`cycleTones`/`cycleSpan` driving
`graf_cycle_attr()` — and that is explicitly *"the Neon one"*, a colour-cycling trick for a
sheet laid out by hue, not multi-frame animation of arbitrary tiles.

So animated tiles are a **feature to add, not port** — and it is worth deciding whether we
want them at all before T2 designs the icon layer, because per-entity frame sequences change
the shape of the tile lookup. Filing as an open question rather than a decision; it is
smaller than §6 decision 4 but lives in the same code.

*Also noted:* the `interface.html` warning about macros is worth heeding — *"Macros are not
the same as in the original Angband. They use X Windows symbols for keypress names… most of
your old macros won't work with AngbandTk."* Whatever we do, use **4.2's** keymap
representation, so macros written in the term build work in the Tk build and vice versa. The
original chose otherwise and told users to live with it.

---

## OBS-28 — The Alternate Editor: conditional tile substitution
*Screenshot: Alternate Editor — a `Reason` menu, a small list of current alternates, a
category strip, and a grid of candidate tiles.*

`alternate.tcl:420-429` gives away what it is. The `Reason` menu is a four-way radio:

| Reason | Meaning |
|---|---|
| `none` | one tile, unconditionally |
| `feature` | pick the tile by the **terrain the thing is standing on** |
| `ident` | pick by whether the object is **identified** |
| `number` | pick by **how many** there are |

So an "alternate" is a **conditional tile substitution**: an unidentified potion draws as a
generic flask and its real tile once known; a stack of five arrows draws differently from
one; a monster on water differs from the same monster on grass. The editor is the authoring
tool for that table — 224 lines of it for dg32.

**Agreed: not in the first run.** And the reasoning is stronger than "we don't need it",
because the editor is a tool for authoring *index-keyed* data we are not carrying forward at
all (plan §1.3). No data to author, no need for the authoring tool.

**But the capability is worth separating from the editor**, because 4.2's coverage is
uneven:

- `ident` — **largely already there.** 4.2 has a full flavour system: unknown potions and
  scrolls take flavour-based appearances, mapped through `graf-*.prf`. That is the same idea
  and it is maintained.
- `number` and `feature` — **no 4.2 equivalent.**

So if quantity- or terrain-conditional tiles are ever wanted, they are a small extension to
the tile lookup, and the data belongs in a **`graf-*.prf`-style text file** — not in a GUI
editor. Which is the same conclusion as `tk/vault/` (§6 decision 3): drop the editors, keep
text data under version control, and let a human edit text.

That now covers all three of the original's tile-authoring tools — **Vault**, **Assign** and
**Alternate** — and the answer is the same for each.

### One thing to keep from the Assign editor, though: the monster grouping

*Screenshot: Assign — tabs Character · Effects · Features · Monsters · Objects, menus
"Assign To" / "Assign What", with monster groups on the left (Ancient Dragons, Angelic
Beings, Birds, Canines, Creeping Coins, Demihumans, Dragons…) and their members on the right,
each showing its current tile.*

Agreed on dropping the editor — nothing we author needs it. But note that its left pane is
the **same monster-group taxonomy** as the Knowledge window's
([OBS-24](#obs-24--the-knowledge-window-six-tabs-three-of-them-zangbands)), and Knowledge is
a T6 deliverable regardless. In the original that grouping came from `r_info.c` (1,737 lines
in the bridge).

**4.2 already has it, as data.**
[lib/gamedata/monster_base.txt](../../lib/gamedata/monster_base.txt) defines **59** bases —
`ancient dragon`, `ainu`, `ant`, `bat`, `bird`, `canine`, `centipede`, `creeping coins`, … —
which is recognisably the same taxonomy, and
[ui-knowledge.c:1159](../../src/ui-knowledge.c#L1159) already builds
`ui_monster_category` groups from it for 4.2's own knowledge browser.

So the grouping is free, and `r_info.c`'s 1,737 lines shrink to "expose 4.2's categories".
Worth knowing before T6 sizes that file.

The five tabs are also a useful reminder that tile assignment covers more than monsters:
**Character, Effects, Features, Monsters, Objects**. If terrain- or effect-tile assignment is
ever wanted, that is the axis list.

---

## OBS-29 — The Color editor: a 2001 toy that is now an accessibility feature
*Screenshots: Color — tabs **List · Monster Bar · Status · Target**, each with a live
preview of the real widget above a 256-swatch palette. List covers List Background, List
Highlight (Active/Inactive) and per-category Inventory Colors; Monster Bar covers the name
plate and the health and mana bars; Status covers **Good / Info / Bad**; Target covers the
active and inactive target markers.*

*"Seems you can change the colors. Not sure why that is useful."* — a fair question, and in
2001 the honest answer was mostly "it isn't". Two reasons it existed:

1. **A palette workaround.** `bugs.html` admits *"Color usage on 256-color monitors is silly,
   the result being that some colors are not as they should be."* On an 8-bit display the
   editor was damage control. That reason is dead.
2. **Customisation for its own sake**, which is a 2001 kind of feature.

### But there is a third reason, and it did not exist as an expectation in 2001

**Colour vision deficiency.** Look at what these tabs actually control:

- **Status** is Good / Info / Bad — rendered green / blue / **red**.
- **Monster Bar** is a **red** health bar next to a blue one.
- **Target** is a **red** marker against a grey one.

Red-versus-green is the most commonly confused pair there is, affecting roughly 1 in 12 men.
A player with deuteranopia cannot read the Status colours *or* judge a health bar at a
glance — and health is the one number in a roguelike you must read instantly. Recolouring is
the accommodation.

The original was smarter than it needed to be here: the roles are named **Good / Info /
Bad**, not "green / blue / red". The semantics survive any recolouring, which is exactly the
property a colour system needs.

**So keep it, and reframe it.** Not "customise your colours" but *"these are the semantic
colour roles, and they are adjustable."* Better still, do not lead with an editor: ship two
or three **validated palettes** — default, high-contrast, colour-blind-safe — and keep the
per-slot editor as the escape hatch behind them. Less work than it sounds and far more
likely to be used.

Note also that **these four tabs are a design-token system**: named semantic slots, grouped
by the widget they belong to, each with a live preview. That is how one would build it today.
Do not flatten it into a list of colours.

### It is a capability 4.2 does not have

[z-color.h](../../src/z-color.h) gives `BASIC_COLORS 29` and `MAX_COLORS 32`, and
`lib/customize/` has font and sound prefs but **no colour pref** — so the term build cannot
recolour anything. The whole of this is additive, and it is one of the clearer cases where
the Tk front end is not merely a prettier face on the same game.

### And the third instance of one pattern

Every tab previews the **real widget** as it will look. That is the same principle as the
tileset preview being a live render ([OBS-02](#obs-02--setup-chooses-the-tileset-before-the-game-starts-with-a-live-preview),
[OBS-20](#obs-20--the-other-menu-three-categories-one-delightful-loop-and-one-more-collision))
and the paper doll showing real slot contents
([OBS-25](#obs-25--the-items-window-is-a-paper-doll-and-it-is-the-best-thing-in-the-front-end)).

Three independent instances make it a house rule worth stating: **anything configurable is
previewed through the real renderer, never through a mock-up.** A preview that goes through
the actual path cannot lie, and cannot rot when the path changes.

---

## OBS-30 — The Font editor, and why it matters more now than in 2001
*Screenshot: Font — a list of **targets** (Autobar Menu, Choice Window, Equipment &
Inventory, Knowledge & Other, Macros, …), then family / size / style (Bold, Italic), a live
"quick brown fox" preview, and "1 item selected" in the status bar.*

Agreed, handy — and three details are worth carrying over:

1. **Fonts are assigned per UI *area*, not globally.** Choice Window, Equipment & Inventory,
   Knowledge & Other, Autobar Menu, Macros. Different areas have different jobs and want
   different sizes.
2. **The target list is multi-select** — "1 item selected" implies more is possible — so you
   can set every text area at once instead of walking a list. Keep that; it is the
   difference between a usable font dialog and a chore.
3. **A live preview**, which is the fourth instance of the pattern in
   [OBS-29](#obs-29--the-color-editor-a-2001-toy-that-is-now-an-accessibility-feature).

### Why this is now more important than it was

A fixed 8pt MS Sans Serif was reasonable on a 96dpi 800×600 display. On a 5K panel,
unscalable UI text is simply unreadable — and larger text is the most-requested
accessibility accommodation there is. So per-area font sizing moves from convenience to
requirement, and **the default cannot be a fixed small point size**; it has to come from the
platform's own text size.

### The trap: font choice on a term pane is a *layout* operation

The original's target list quietly mixes two different kinds of thing:

- **Native widget areas** — Choice Window, Knowledge, Macros. Changing the font reflows text
  inside a widget. Cosmetic.
- **Term panes** — the Main window. In a term view the font *is* the grid: the character
  cell determines how many rows and columns of dungeon fit, so changing it resizes the
  playfield.

Under decision 1 (one window, Ttk panes) that second case is sharper still, because a pane's
font changes how much map its pane shows. The two need distinguishing in the UI — a term
pane's font control belongs next to its tile-size control, not in a list beside
"Autobar Menu".

### What 4.2 offers

There is precedent but not a match: `lib/customize/` carries `font.prf`, `font-win.prf`,
`font-x11.prf`, `font-sdl2.prf`, `font-gcu.prf` and `font-ibm.prf` — **per-platform term
fonts**, not per-area UI fonts. So the term-pane half has somewhere to live; the native-area
half is new, and should be our own preference store rather than bent into a `font-*.prf`.

*Artefact, not a bug:* the family list shows "MS UI Gothic" six times over. That is Wine's
font enumeration duplicating entries, not the application.

### "It does not appear to do anything" — probably the target, not the editor

There is no Apply button because it applies **live**: `font.tcl:565` does
`Value font,$item $font`, and the value manager notifies every widget that registered a
client for that key (the pattern `NSValueManager::AddClient font,knowledge …` seen in
`character-window.tcl`).

The sixteen targets are `autobar · choice · inventory · knowledge · macros · magic ·
message · messages · misc · miscPopup · monster · options · recall · status · statusBar ·
store` — and `InitModule` does `selection set 0`, so **`autobar` is selected by default**.
That is what the screenshot shows. The Autobar is an optional auto-toolbar
(`autobar.tcl`) and it is not visible in any capture so far, so changing its font changes
nothing you can see.

**Testable:** select **Choice Window** — second in the list, and the Choice window is on
screen in every screenshot — set the size to 16, and its item text should change
immediately.

The other candidate is that a window registers its font client only while it exists, so a
closed window's font changes the stored value and applies on next open. Testing with
`choice` separates the two.

---

## OBS-31 — The Keymap editor: right idea, 1990s execution
*Screenshot: Keymap — Open/Save toolbar, a picture of a US keyboard with one key outlined
and one shown in green, an `Action:` text field, and Normal / Shift / Control radio buttons.*

*"Looks very 1990s and user unfriendly."* Agreed — but the split matters, because one half
is a genuinely good idea and only the other half is dated.

**Worth keeping: the keyboard picture.** It answers the question a list cannot — *which keys
are still free?* The green key is bound, the rest are not. That is the whole reason to draw
a keyboard instead of listing bindings, and it is a better affordance than 4.2's term UI
offers.

**What makes it unfriendly**, specifically:

1. **You type the action as an encoded string.** `interface.html`: *"Type an encoded action
   into the Action Entry… `\e` — Escape, `\s` — Space, `\` — Backslash."* The user has to
   know the encoding language.
2. **Normal / Shift / Control are radio buttons** rather than just pressing the modifier.
3. **A bound key does not say what it does** without selecting it.
4. **Open / Save** implies hand-managing pref files.
5. **US-ASCII layout only** — awkward for a program that ships a Japanese localisation.

### The fix falls out of work already planned

**Capture, do not encode.** Press the chord to select the key; then pick the action from the
**command list** — which [OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)
established 4.2 already provides as data, with human-readable names
([ui-game.c:388](../../src/ui-game.c#L388)). So the `Action:` text box becomes a searchable
command picker, and nobody types `\e` again.

Same table, second consumer — and the payoff compounds: because menu accelerators come from
`cmd_lookup_key()`, a key rebound here **immediately shows the new key in the menus**. The
two features make each other work.

Keep Open/Save as an export path, not the primary interaction. Draw the keyboard from the
platform's actual layout rather than a hard-coded US one. And hover a bound key to see the
command it runs.

*One firm constraint:* `interface.html` admits the original's macros *"use X Windows symbols
for keypress names… most of your old macros won't work with AngbandTk."* Use **4.2's**
keymap representation, so bindings written in the term build work in the Tk build and back
again. The original chose otherwise and told users to live with it.

---

## OBS-32 — The 16 default macros are a workaround, and 4.2 removed the need for them
*Screenshot: Macros — Open/Save toolbar, 16 entries, `Trigger:` and `Action:` fields.
`Control-1 → \e\e\\+1`, `Control-2 → \e\e\\+2`, …, `End → \e\e\e\\.1`,
`Down → \e\e\e\\.2`.*

*"Everyone was a fan of those in the 1990s."* — and the sixteen shipped defaults show exactly
why, because they are not player conveniences. They are the front end **papering over key
handling the game did not have.**

Decoded:

| Encoding | Meaning |
|---|---|
| `\e` | Escape — repeated two or three times to bail out of any prompt first |
| `\\` | a literal backslash, which in Angband prefixes a command to **bypass keymaps** |
| `\+1` … `\+9` | `+` is *alter* — tunnel / open / disarm — with a direction digit |
| `\.1`, `\.2` | `.` is *run*, with a direction digit |

So `Control-1` through `Control-9` are **alter in a direction**, and the arrow and keypad
keys are **run in a direction**. Sixteen macros existed so Control+keypad and the arrow keys
would do something sensible, because 2.4.0-era Angband had no native handling for them.

**4.2 handles all of it natively** — arrow keys, keypad, `CMD_ALTER`, running — so the
default set is obsolete. **Ship zero default macros.** And read that list as a retrospective
audit of *what players had to macro around*: directional alter and directional run, both now
built in.

### What this means for macros as a feature

There is a real point behind the joke. Macros were how you compensated for an interface that
could not express what you wanted. A front end with menus that show their accelerators, a
paper-doll equipment screen, clickable inventory and a mouse-driven map removes most of the
*need*. Macros become a power-user convenience — a spell sequence someone repeats fifty
times a session — rather than a survival tool.

So: **keep the capability, drop the defaults, do not make it a headline.** It belongs in
Preferences next to Keymap, which is where the original put it.

### Same encoding problem, same fix

The `Action:` field takes the same encoded string as the Keymap editor
([OBS-31](#obs-31--the-keymap-editor-right-idea-1990s-execution)), so it inherits the same
answer: build the action from T3's command list instead of making the user type `\e`.

The original got the *trigger* half right — `interface.html`: *"Point the mouse over the
Trigger Entry and then type a new keypress"* — so triggers are captured and actions are
typed. Capture both.

*Keep the `\\` idiom* in whatever representation we choose. "Run this command ignoring
keymaps" is genuinely necessary once users can rebind keys: a macro expanding to a keymapped
key would otherwise recurse.

---

## OBS-33 — The Music window: a MOD player, and the feature is easier now than it was
*Screenshot: Music — `Use Music` · `Randomize` · `Set Directory…` · `Refresh Module List` ·
`Close Ctrl+W`, a volume slider at 100, a Position scrollbar, and an empty list.*

*"Looks like you could map it to some music files and play some AC/DC during the game?"* —
**not as built, but yes in the version we would write, and more cheaply than in 2001.**

As built it is a **tracker-module player**. "Refresh Module **List**" is the giveaway, and
`ReadMe_Music.txt` names the formats: Protracker `.mod`, Scream Tracker 3 `.s3m`,
FastTracker 2 `.xm`, Impulse Tracker `.it`, `.mtm`. BASS was chosen because it played
trackers well. No AC/DC — chiptunes. The list is empty because no module directory is set
and the archive ships no modules.

But look at the *shape* of the feature: point it at a directory, randomise the order, set a
volume, scrub a position. That is a music player, and the only thing tying it to trackers is
BASS.

### It costs almost nothing now

4.2 already links **SDL2_mixer** for sound
([SDL2_Sound.cmake](../../src/cmake/macros/SDL2_Sound.cmake)), and
[snd-sdl.c](../../src/snd-sdl.c) already uses its **music** channel — `Mix_Music`,
`Mix_LoadMUS`, `Mix_PlayMusic` are all in there at lines 41, 134 and 190. SDL2_mixer plays
MP3, OGG, FLAC and WAV, and provides volume and position control.

So "point at a folder of your own music, shuffle, volume" needs no new dependency
whatsoever — which is a striking inversion, since [OBS-03](#obs-03--music-is-bass-and-only-bass)
had music down as the one thing to drop outright. **Drop BASS, keep the idea.**

### And it has zero licensing exposure

This is the reason it is worth a moment's thought rather than a shrug. We ship **no music** —
the player supplies their own files. Compare
[§1.4](phase3-tcl-tk-frontend.md): the archived sound packs cannot ship because they are
personal-use-only material. A "play your own music" feature has none of that problem,
because there is nothing to license.

**Recommendation:** T9 or later, off by default, and small — a directory, shuffle, volume,
next/previous. Not a headline feature, but a genuinely liked one, and about as cheap as a
feature gets when the audio library is already linked and already has the API.

*One detail to keep:* `Randomize`. A fixed play order gets old fast in a game session that
runs for hours.

---

## OBS-34 — The Options window: 11 categories against 4.2's 5, and two sources in one list
*Screenshot: Options (Interface) — a category list on the left (Interface · Command · Prompt
· Dungeon · Monster · Object · Disturbance · Efficiency · Misc · Cheating · **Other**), the
page's checkboxes on the right, and a description pane below. Reached by `=` as well as the
menu.*

Agreed that this is the most useful of the preference windows, and it is the one
[OBS-13](#obs-13--birth-options-four-tabs-and-the-wilderness-switch-surfaces) already says to
**generate** rather than hand-author. Two refinements from seeing it.

### 1. 4.2's page taxonomy is too coarse for a GUI

The original categorises eleven ways. 4.2 has **five** pages —
[option.h:30](../../src/option.h#L30): `OP_INTERFACE`, `OP_BIRTH`, `OP_CHEAT`, `OP_SCORE`,
`OP_SPECIAL` — and lumps nearly everything into `OP_INTERFACE`.

But *Prompt*, *Disturbance*, *Efficiency*, *Monster* and *Object* are genuinely different
concerns. "When does the game interrupt me?" and "how are objects described?" do not belong
on one page just because both are technically interface.

So: **take the labels and descriptions from `list-options.h`, but supply our own grouping.**
That is the third instance of the same pattern — the menu map
([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)) and the
Knowledge tab set ([OBS-24](#obs-24--the-knowledge-window-six-tabs-three-of-them-zangbands))
were the first two. **4.2 supplies the leaves; the arrangement is ours.** Worth stating once
as a rule rather than rediscovering per window.

The generator therefore needs a small declarative **category map** (option name → category),
sitting beside the menu map. Options not in it fall through to a sensible default, so a new
4.2 option can never go missing — only uncategorised.

### 2. Game options and front-end options are merged, and that is right

Look at what is on the Interface page:

| Option | Owner |
|---|---|
| Show dungeon level in feet | **game** (`depth_in_feet`) |
| Plain object descriptions | **game** |
| Show labels in object listings | **game** |
| Show weights in object listings | **game** |
| Show details in certain sub-windows | **front end** |
| Show icons in inventory/store list | **front end** |
| Keep character centered in the display | **front end** |
| Allow animation | **front end** (`allow_animation`, referenced in `icon.h`) |

Four of eight are the front end's own, interleaved with the game's — and the `Other`
category at the bottom of the list is a further front-end bucket. The player cannot tell
which layer owns a setting, and **should not have to**.

So the window merges **two sources**: 4.2's `list-options.h`, plus a front-end option table
of our own (tile set and size, animation, icons in lists, centre-on-player, pane fonts,
colour palette). Both feed one categorised presentation.

That also gives the front-end options somewhere principled to live, which matters because
several decisions in this log create them — the tileset choice
([OBS-02](#obs-02--setup-chooses-the-tileset-before-the-game-starts-with-a-live-preview)),
the colour palette
([OBS-29](#obs-29--the-color-editor-a-2001-toy-that-is-now-an-accessibility-feature)), per-pane
fonts ([OBS-30](#obs-30--the-font-editor-and-why-it-matters-more-now-than-in-2001)) and the
music player ([OBS-33](#obs-33--the-music-window-a-mod-player-and-the-feature-is-easier-now-than-it-was)).

*And it explains the dead controls.* `interface.html` warns that *"options from the non-Tk
version of the game can be displayed, but these options have no effect."* With two sources
hand-merged and no single authority, drift was inevitable. Generating from both tables makes
a dead control structurally impossible — an option that exists is shown, one that does not
cannot be.

---

## OBS-35 — The Sound editor: skip it, but note what the earlier decision did *not* cover
*Screenshot: Sound — menus `Sound` / `Config` / `Assign` / `Directory`; tabs **Events ·
Monster Attacks · Monster Spells · Martial Arts**; panes for Configuration (checkable
"Default Sounds"), Events (checkable, "Action failed", "Enter a store", …), Assigned Sounds,
Directory (`0: C:\OmnibandTk-1.4\lib\sound`) and Sound Files.*

Confirmed skip — but the screenshot sharpens the earlier decision, because **this window is
not the per-monster tier.**

The four tabs are exactly the `grund.snd` groups from
[§1.4](phase3-tcl-tk-frontend.md) minus one: `event`, `monster_attack`, `monster_spell`,
`martial_art` — all **symbolic** families. There is no Monsters tab, because per-monster
assignment lives in the *Assign* window (hence the `Assign` menu here, jumping across). So:

- **The settled "no per-monster reassignment" decision applies to *Assign*,** and stands.
- **This window is the 211 symbolic events**, which we *are* mapping onto 4.2's 149.

Two different things, and both end up dropped — but for different reasons, and it is worth
being precise so the *event vocabulary* work is not dropped along with them.

### Why skip this one too

The data model is richer than 4.2's, but not usefully so:

| Feature | 4.2 |
|---|---|
| Multiple stackable named configurations | Not directly — but `sound.prf` is one file, and a second could be loaded |
| Numbered sound directories a config references | `lib/sounds/`, one place |
| **Multiple samples per event, chosen at random** | **Already supported** — `sound:EVENT:sample1 sample2` |

The one genuinely nice feature — several samples per event so a hit does not sound identical
fifty times — 4.2 already has.

So the answer is the same as for Vault, Assign and Alternate
([OBS-28](#obs-28--the-alternate-editor-conditional-tile-substitution)): **the mapping is
text under version control, and a human edits the text.** `sound.prf` is already that file,
already loaded, already supports the multi-sample form. No editor.

What survives from this window is what was already decided: a **volume control and an
on/off**, and nothing else ([OBS-04](#obs-04--sound-offers-three-backends-all-of-them-windows)).

**That is now four editors dropped for one consistent reason** — Vault, Assign, Alternate,
Sound. All four authored mapping data; in every case the replacement is a text file in the
repository. Worth stating as a principle: *ZangbandTK ships no data editors.* Mapping data
lives in `lib/gamedata/` and `lib/customize/` where it can be diffed, reviewed and shipped —
not in a GUI that writes files marked `# Automatically generated. Do not edit.`

---

## OBS-36 — The Sprite editor closes the animation gap, and the spec is three fields
*Screenshot: Sprite Editor — five sprites in a list (**actually animating**), a **Frame Delay
Factor** slider, a **Reverse** checkbox, and the tile palette on the right.*

This resolves the open gap in
[OBS-27](#obs-27--the-ten-preferences-and-what-survives-of-them). Zangband's sprites are
genuine multi-frame tile animation, and the data model is tiny:

| Field | Meaning |
|---|---|
| ordered frame list | tiles picked from the palette, in sequence |
| **Frame Delay Factor** | playback speed |
| **Reverse** | ping-pong (1→n→1) instead of loop (1→n→1→n) |

Three fields. And the whole `dg32-sprite` config is **74 lines**. So this is a small feature,
not a subsystem — animated water, guttering torches, a flickering fire elemental.

**4.2 has nothing equivalent.** Its only animation-adjacent mechanism is the neon set's hue
shimmer — `cycle:10:4:9` in [lib/tiles/list.txt](../../lib/tiles/list.txt), described at
[grafmode.h:38](../../src/grafmode.h#L38) as `cycleHues`/`cycleTones`/`cycleSpan` feeding
`graf_cycle_attr()`, and explicitly *"the Neon one"*: a colour rotation for a sheet laid out
by hue, not frame animation of arbitrary tiles.

### The recommendation splits cleanly along the principle just established

- **Implement the capability.** It is three fields and 74 lines of data, it is visible, and
  it is the difference between a tileset that looks painted and one that looks alive.
- **Skip the editor**, per [OBS-35](#obs-35--the-sound-editor-skip-it-but-note-what-the-earlier-decision-did-not-cover).
  A sprite is a line of text: name, frames, delay, reverse. It belongs in a `graf-*.prf`-style
  file alongside the tile assignments.

**But it must land in T2's design, not after.** Frame sequences change the tile lookup's
*shape* — from "index → tile" to "index → tile *or* animation", with a per-frame clock behind
it. Retrofitting that into a finished icon layer is exactly the kind of change that ends up
threaded through everything. It also interacts with §6 decision 5 (composable modes), since
an animation is per-icon-type just as an alternate is.

So: T2 designs the icon layer knowing that a tile lookup may return a sequence. Whether we
author any sprites in the first pass is a separate, cheap question.

*One thing worth confirming while the reference runs:* whether animation was frame-synced to
game turns or to real time. The `Frame Delay Factor` slider and the `allow_animation` option
([OBS-34](#obs-34--the-options-window-11-categories-against-42s-5-and-two-sources-in-one-list))
suggest real time, which is the right answer for water but debatable for a monster.

---

## OBS-37 — The Window menu, and "Arrange Windows" is the 800px maths meeting a 5K screen
*Screenshot: Window ▸ Arrange Windows… · Maximize Windows… ‖ Save Window Positions · Load
Window Positions · ✓AutoSave Positions ‖ ✓Choice Window · Message Window · ✓Messages Window ·
✓Micro Map Window · ✓Misc Window · ✓Recall Window.*

*"That Arrange Windows… option causes the windows to fly everywhere on the screen! Might be
a CrossOver issue."*

**Almost certainly not CrossOver — it is the application's own layout arithmetic being handed
a screen thirty times larger in area than it was written for.** And it is direct confirmation
of [OBS-16](#obs-16--two-separate-mechanisms-hold-the-windows-together-and-one-may-be-a-wine-artefact).

`Arrange Windows` calls `NSWindowManager::ArrangeAll`, which asks every registered window for
its geometry via the `geomCmd` it handed over at registration. Here is what one of those
actually computes (`recall.tcl`, `NSRecall::GeometryCmd`):

```tcl
scan [angband system workarea] "%d %d %d %d" left top right bottom
if {[Platform unix]} {
    set left 0 ; set top 0
    set right [winfo screenwidth .] ; set bottom [winfo screenheight .]
}
set x [NSToplevel::FrameLeft $winMain]
…
```

So each window's position is derived from **the desktop work area** and **the main window's
frame**, with the `-N means N pixels in from the right edge` convention seen at
`window-manager.tcl:92-103`. Written for an 800×600 desktop that produces the tidy
six-window layout in the old plates. Given a modern work area, the same arithmetic dutifully
pushes windows to the far edges of a very large desktop — which is what "flying everywhere"
looks like. A real 5K Windows machine would do the same.

CrossOver's only contribution is *reporting* the work-area size. Genuine Wine artefacts look
different — see the `wm transient` follow-the-parent behaviour in OBS-16, or the duplicated
font families in [OBS-30](#obs-30--the-font-editor-and-why-it-matters-more-now-than-in-2001).

**Under decision 1 this feature ceases to exist**, which is the cleanest possible argument
for that decision: `Arrange Windows` and `Maximize Windows` are both attempts to manage a
problem that panes do not have. Panes are always arranged.

### Worth keeping: AutoSave Positions

It is **on by default**, so layout persists without the player thinking about it — and
`Save`/`Load Window Positions` are the manual escape hatch behind it. That is the right
default, and the paned equivalent is auto-saving sash positions, with named layouts as the
explicit form ([OBS-16](#obs-16--two-separate-mechanisms-hold-the-windows-together-and-one-may-be-a-wine-artefact)).

### A naming trap: "Message Window" and "Messages Window" are different windows

Both are in the menu, and they are not the same thing. `interface.html` distinguishes them:
the **Message Window** carries the current message and the `-more-` prompt; the **Messages
Window** is the scrolling log. In this session's captures the singular one is *off*, because
the Main window's top line is doing that job.

Two windows whose names differ by one letter is a bug in the interface, not a feature. Name
them **Message Line** and **Message Log**.

### One correction to DEC-12's platform list

DEC-12 says `plat.c` abstracts four things — a font chooser, X-window-to-HWND conversion, a
`system` command, and a millisecond timer. **There is a fifth: the desktop work area**, via
`angband system workarea`, and it is load-bearing for every window's geometry. On macOS the
equivalent is the screen's visible frame minus menu bar and Dock. Small, but it belongs on
the list rather than being discovered during T1.

---

## OBS-38 — Help and Book, and the philosophy that ties the whole front end together
*Screenshots: Help ▸ Help `?` · Tips · About ZAngbandTk…  ·  Book ▸ Sign of Chaos ▸ · Chaos
Mastery ▸ · Chaos Channels ▸ · Armageddon Tome ▸.*

**Help** is fully accounted for already: `Help` `?` is the TkHtml browser
([OBS-08](#obs-08--the-help-system-is-tkhtml-not-winhelp), replaced by Sphinx), `Tips` is
the 32-tip window ([OBS-07](#obs-07--the-interaction-model-was-written-down)), `About` is
`about.tcl`. And the key matches 4.2 exactly:
[ui-game.c:184](../../src/ui-game.c#L184) — `{ "Help", { '?' }, CMD_NULL, do_cmd_help }`.

**Book** is realm-structured: the four Chaos realm books as submenus, each presumably
listing its spells. So casting is menu → book → spell rather than `m` → book letter → spell
letter. Everything needed is in 4.2:

- [class.txt](../../lib/gamedata/class.txt) already carries `book:` entries tagged with
  their **realm** — and one of them is `[Pattern Sorcery]`, so the Amber flavour is in the
  tree already.
- `spell_okay_to_cast()`, `spell_okay_to_study()`, `spell_okay_to_browse()`
  ([player-spell.h:49](../../src/player-spell.h#L49)) — three predicates, so greying comes
  free exactly as the item prereqs do in
  [OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design).

### The thing worth noticing: it lists **four** books, and the character owns one

That is not a bug. And it is the third time the same choice has appeared:

| Screen | Shows the whole space | Marks what you have |
|---|---|---|
| **Paper doll** ([OBS-25](#obs-25--the-items-window-is-a-paper-doll-and-it-is-the-best-thing-in-the-front-end)) | every equipment slot, drawn empty | filled slots hold an item |
| **Flags table** ([OBS-22](#obs-22--the-character-windows-four-other-tabs-three-work-one-is-switched-off)) | all ~40 flags listed | dots where you have them |
| **Book menu** | all four realm books | the one in your pack is usable |

So the front end consistently **shows the whole space and marks your position in it**, rather
than showing only your current state. An Armageddon Tome you cannot yet read is on the menu
because knowing it exists is part of learning the game.

**This is the real thesis of the front end**, and it is a sharper statement than "menus so
you don't memorise keys". Accelerators beside menu items
([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)) are the
same idea applied to the keyboard: the UI teaches the game's shape instead of merely
reflecting your state. Every design decision in T4–T7 should be checked against it.

### And it reconciles the one critique

[OBS-22](#obs-22--the-character-windows-four-other-tabs-three-work-one-is-switched-off) faults
the Flags table for a grey wall of unheld flags with five distant dots. That is not an
argument against showing the whole space — the paper doll shows the whole space and reads
beautifully, because an empty box in a body outline is *instantly* distinguishable from a
filled one.

So the rule is two-sided: **show the whole space, and make the delta unmissable.** The Flags
table gets the first half right and the second half wrong; the paper doll gets both. That is
the difference between the two, and it is a checkable standard rather than a matter of taste.

---

## OBS-39 — Two realms in the Book menu, and M9's structure turns out to be done
*Screenshot: a Human **Priest** ("Believer") with the Book menu showing two divider-separated
groups — **Book of Common Prayer · High Mass · Book of the Unicorn · Blessings of the
Grail**, then **Beginner's Handbook · Master Sorcerer's Handbook · Pattern Sorcery ·
Grimoire of Power**. SP is 2, and the message line reads "You can learn 1 more prayer."*

Two realms, four books each, separated by a menu divider. That confirms
[OBS-14](#obs-14--the-autoroller-and-what-42-replaced-it-with)'s finding from the other
direction: `REALM_CHOICES 2` in [player.h:473](../../src/player.h#L473) is exactly the
Realm 1 / Realm 2 pair, and the menu groups by realm rather than flattening.

### The books are all already in the tree

Checked every name against [class.txt](../../lib/gamedata/class.txt), and then enumerated
what is there. **Six realms, four books each, twenty-four books — all present:**

| Realm | Books |
|---|---|
| arcane | Cantrips for Beginners · Minor Arcana · Major Arcana · Manual of Mastery |
| chaos | Sign of Chaos · Chaos Mastery · Chaos Channels · Armageddon Tome |
| death | Black Prayers · Black Mass · Black Channels · **Necronomicon** |
| life | Book of Common Prayer · High Mass · Book of the Unicorn · Blessings of the Grail |
| nature | Nature's Gifts · Nature's Wrath · Nature Mastery · Call of the Wild |
| sorcery | Beginner's Handbook · Master Sorcerer's Handbook · Pattern Sorcery · Grimoire of Power |

That is Zangband's complete realm set, matching both this screenshot and the Chaos four from
[OBS-38](#obs-38--help-and-book-and-the-philosophy-that-ties-the-whole-front-end-together).

**So M9's realm and book *structure* is already done at the data level.** The plan's T8 note
says "not before M9 settles" — the structure has settled; what may still move is spell
content and balance. Worth re-reading that sequencing note against reality rather than
leaving T8 blocked on a milestone that is further along than assumed.

### Amber, already right in three places — and wrong in one

Three of these are on target without anyone doing anything: **Book of the Unicorn** (the
Unicorn of Amber, counterpart to the Serpent of Chaos), **Blessings of the Grail**, and
**Pattern Sorcery** — the Pattern again, as in `pattern16.gif`
([§1.3](phase3-tcl-tk-frontend.md)). Zangband carried real Amber material in its book names,
the same way its mandatory quests were Oberon and the Serpent
([OBS-24](#obs-24--the-knowledge-window-six-tabs-three-of-them-zangbands)).

One is not: the death realm runs **Black Prayers · Black Mass · Black Channels ·
Necronomicon**. Three generic, one **Lovecraft** — precisely the drift DEC-01 treats as a
defect rather than a legacy.

It is also the cheapest possible correction: **one book name**, in one data file, with no
mechanical consequence. Amber has plenty to draw on for a death-realm capstone — the Courts
of Chaos, the Logrus, Suhuy's teachings. Worth raising with M9 now, because renaming a book
before anyone writes documentation or a savefile depends on it costs nothing, and afterwards
it costs a migration.

---

## OBS-40 — The Book window is a table view of data 4.2 already computes
*Screenshot: "Life Book - Book of Common Prayer" — a toolbar, a vertical strip of **book
icons** on the left (one per carried book), and a table: letter · **Name · Level · Mana ·
Fail · Comment**. Rows run `a) Detect Evil 1 / 1 / 9% / unknown` down to
`h) Satisfy Hunger 7 / 5 / 68% / unknown`, in three visibly different colours.*

Reached by `Browse (b)` from Inven → Magic
([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)).

### The columns and the colours are 4.2's, already

[ui-spell.c:249](../../src/ui-spell.c#L249) sets its header to:

```
"Name                             Lv Mana Fail Info"
```

The **same five columns**, in the same order. And `spell_menu_display`
([ui-spell.c:64](../../src/ui-spell.c#L64)) already defines the colour semantics the
screenshot is showing — six states, not the three visible here:

| Condition | Comment | Colour |
|---|---|---|
| `slevel >= 99` | `(illegible)` | `COLOUR_L_DARK` |
| `PY_SPELL_FORGOTTEN` | ` forgotten` | `COLOUR_YELLOW` |
| learned **and** worked | `get_spell_info()` — damage, duration | `COLOUR_WHITE` |
| learned, not yet used | ` untried` | `COLOUR_L_GREEN` |
| not learned, level reached | **` unknown`** | `COLOUR_L_BLUE` |
| level not reached | — | dim |

That accounts for the screenshot exactly: every spell reads `unknown` because none is
learned yet, and the dim rows are the ones above the character's level.

**So the Book window is: expose `spell_menu_display`'s logic through the bridge, render as a
Ttk treeview with five columns.** Nearly free — and strictly *better* than the term view in
three ways that cost nothing:

1. **Columns become sortable.** "Which of these can I actually afford right now?" is a Mana
   sort.
2. **Rows become clickable**, so casting is a double-click rather than a letter.
3. **4.2 distinguishes six states where the original showed three.** `get_spell_info()`
   fills the Info column with real damage and duration once a spell has been used, which the
   original's "Comment" column never carried.

### The one genuinely new affordance: the book strip

The left-hand column of book icons switches between the books the character is carrying
**without closing the window**. Small, cheap, and the sort of thing that disappears in a
term UI because there is nowhere to put it.

It is also another instance of
[OBS-38](#obs-38--help-and-book-and-the-philosophy-that-ties-the-whole-front-end-together)'s
rule — the strip shows the books you *have*, while the Book **menu** shows all four in the
realm. Same information, two scopes, each right for its place: the menu teaches what exists,
the window operates on what you hold.

---

## OBS-41 — The Recall window is a universal detail pane, and it fixes the menu's worst flaw
*Screenshots: Book ▸ Book of Common Prayer ▸ showing eight spells with per-spell letters
(`a Detect Evil` … `h Satisfy Hunger`), while the **Recall** window displays
"Detect Evil: Level 1 Mana 1 Fail 9% / untried".*

Two findings here, and they solve each other.

### 1. The idiom I had not named: hover anything, Recall explains it

`recall.tcl` exposes a **typed detail API** — `NSRecall::RecallArtifact`, `RecallMonster`,
`RecallObject`, `RecallObjectKind`, `RecallSpell`, `RecallQuest`, `RecallMindcraft`,
`RecallStack` — and **25 of the 69 scripts call into it.**

So the Recall window is not "the monster memory window". It is a **single universal detail
pane, driven by whatever the pointer is over**, and it is the front end's core interaction
idiom. Every instance found so far is the same convention:

| Point at… | Recall shows |
|---|---|
| a map grid | the look description; over a monster, its memory (and the health bar appears) |
| an inventory row | the item's memory |
| an equipment icon in the Flags tab | the item's description |
| **a spell in the Book menu** | level, mana, fail %, and its state |
| a knowledge list entry | that entry's recall |

**This changes how T4–T7 should be built.** Not "each window carries its own detail display",
but **one detail pane with a typed API, and everything feeds it.** That is one widget to get
right instead of eight, and it means a novice has exactly one place to look. It also explains
why Recall grows on pointer-enter and shrinks on leave
([§1.5](phase3-tcl-tk-frontend.md)) — it is in constant use, so it has to be both
unobtrusive and readable on demand.

Worth adding to T3's bridge work: the typed recall API is the shape to expose, not
per-window ad-hoc queries.

### 2. "Only Detect Evil is selectable, and the menu is not clear what is enabled"

Correct, and it is the **third** instance of the original knowing something and failing to
show it:

| Screen | Knows | Fails to show |
|---|---|---|
| Flags table ([OBS-22](#obs-22--the-character-windows-four-other-tabs-three-work-one-is-switched-off)) | which flags you have | the delta, in a grey wall |
| Notes tab ([OBS-22](#obs-22--the-character-windows-four-other-tabs-three-work-one-is-switched-off)) | note-taking is off | why the tab is empty |
| **Book menu** | which spells are castable | any visible difference |

Here the fix is nearly free, because **native menus already solve it.** Ttk and the platform
menu bar grey out disabled items to a platform standard the user already reads fluently —
which is one more reason not to hand-draw menus
([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)).

But greying is only half a fix for spells, because "disabled" has **several different
causes**: not learned, level too low, not enough mana, blind or confused. A grey item that
does not say *which* leaves the player guessing.

**And finding 1 is the other half.** The Recall pane already shows level, mana, fail and
state on hover — so the rule is: **grey it out, and let hover explain why.** 4.2 supplies
both halves already; `spell_okay_to_cast()` decides the greying
([player-spell.h:49](../../src/player-spell.h#L49)) and `spell_menu_display`'s six-state
logic supplies the reason ([OBS-40](#obs-40--the-book-window-is-a-table-view-of-data-42-already-computes)).

That pattern generalises beyond spells: **anything disabled should be visibly disabled and
able to say why.** It is the actionable form of "make the delta unmissable"
([OBS-38](#obs-38--help-and-book-and-the-philosophy-that-ties-the-whole-front-end-together)),
and together with the empty-state rule it covers every "the UI knows but does not say"
failure in this log.

---

## OBS-42 — The File menu: three entries, one semantic trap, and two accelerator namespaces
*Screenshot: File ▸ Save `Ctrl+S` ‖ Quit With Save `Ctrl+X` · Quit.*

Two of the three map onto 4.2 exactly:

| ZangbandTk | 4.2 |
|---|---|
| Save `Ctrl+S` | `{ "Save and don't quit", { KTRL('S') }, save_game }` |
| Quit With Save `Ctrl+X` | `{ "Save and quit", { KTRL('X') }, textui_quit }` |
| **Quit** *(no accelerator)* | **no equivalent** — see below |

### The trap: "Quit" is not 4.2's `Q`

4.2's `Q` is `{ "Retire character and quit", { 'Q' }, textui_cmd_retire }` — it **permanently
ends the character**, scored and filed. ZangbandTk's plain `Quit` is `QuitNoSave`
(`init-startup.tcl:297`), which abandons the session without saving.

Two entirely different destructive acts, and **neither should ever be labelled just "Quit"**.
This is the same class of error as `Destroy` → `Ignore`
([OBS-19](#obs-19--inven--use-and-one-semantic-trap)): a familiar word attached to different
semantics. Label them for what they do — *"Quit without saving"* and *"Retire character"* —
and confirm both.

Note also the original's ordering: the destructive entry sits **last, unlabelled and without
an accelerator**, directly below "Quit With Save". One slip in a menu is the difference
between saving and discarding. Separate them.

### The real finding: there are two accelerator namespaces, and the original conflated them

| Namespace | Examples | Rule |
|---|---|---|
| **Game keys** | `^F` feeling · `^P` messages · `^Q` quest status · `^T` time of day · `^S` save · `^X` save+quit · every letter command | **Stay literal Control.** These *are* the keymap, and the menus display them to teach them ([OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)) |
| **Application commands** | Save · Quit · Close Window · Preferences | **Follow the platform.** ⌘S, ⌘Q, ⌘W, ⌘, — and on macOS, Quit and Preferences belong in the *application* menu, not File |

The original used `Ctrl+S` for Save and `Ctrl+W` for Close Window
([OBS-33](#obs-33--the-music-window-a-mod-player-and-the-feature-is-easier-now-than-it-was)),
which on Windows was right and on macOS is wrong. But the game's own `^F`, `^P`, `^Q`, `^T`
must **not** move to Command, or they stop being the keys the manual and the term build
describe.

**So bind both, and show the platform one.** ⌘S in the menu, `^S` still working as a game
key; ⌘Q as *save-and-quit*, `^X` still working. There is no collision because the two
namespaces use different modifiers — which is only true if we resist "tidying" the game keys
onto Command.

**And ⌘Q must be safe.** On macOS ⌘Q is muscle memory; it cannot be the one that discards
progress. Map it to save-and-quit, and put quit-without-saving behind a confirmation
somewhere it cannot be hit by accident.

*Menu audit status:* File · Inven · Book · Action · Other · Window · Help all captured.
**Tool** is the last one — and the only one whose contents cannot be predicted from 4.2's
tables, since it is presumably where the front end's own instruments live.

---

## OBS-43 — The uneven menus are unfinished work, not a design philosophy — and the change log proves it
*Observed: "some menus are complete… they bring up dialogs to do stuff, like the equip
command, browse book. But other menus go back to the main window standard keyboard
interface, e.g. eat food. I would say the developer just did not finish it."*

**Correct.** `tk/doc/changes.html` is 76 KB of release notes across **20+ releases from
August 1998 to August 2001**, and it documents exactly this conversion happening one command
at a time. The decisive entry, from 2.2.8r2 (April 2000):

> The new Book Window displays all the available books in a single window. The
> **"Browse (b)" command no longer asks for which book to browse.**

That *is* the pattern described: a command that used to fall back to a term prompt got a
bespoke window, and the prompt was eliminated. The author was systematically doing this
conversion. He reached Browse in April 2000, the Items window earlier, stores earlier still —
and never reached Eat Food. Development stopped in August 2001, with the last release adding
an isometric view and window-integration polish rather than more command dialogs.

So it is a **trajectory that ran out of time**, not a considered mixed model.

### One nuance that matters for planning

`Eat Food` is not as bare as it looks. The Choice Window arrived in 2.2.7r1 (October 1999)
as the **generic** answer:

> The new Choice Window displays all the choices which used to be displayed in the Recall
> Window. When the game is not asking the user to make a choice, the character's inventory or
> equipment is displayed.

So when the game prompts for an item, the Choice pane lists the valid options and they are
clickable ([§1.5](phase3-tcl-tk-frontend.md), mouse rules 5 and 6). Nothing is keyboard-only.
There were **two conversion strategies running at once**:

| Strategy | Mechanism | Commands |
|---|---|---|
| **Generic** | the prompt appears; Choice/Recall list the valid options, clickable | everything, by default |
| **Bespoke** | a dedicated window; the prompt never appears | Equip (paper doll), Browse (Book window), stores, knowledge |

### The planning conclusion: do not simply "finish it"

The tempting reading is "convert every command to a bespoke dialog". That would be a great
deal of work for little gain, and it is not what makes the good screens good.

**The test is what the command asks of the player:**

- **Choosing from a list** — eat, quaff, read, drop, throw, fuel. A *good* generic selection
  pane serves these completely, and a bespoke dialog for each adds nothing but code. Make
  the generic pane excellent: icons, weights, the detail pane on hover
  ([OBS-41](#obs-41--the-recall-window-is-a-universal-detail-pane-and-it-fixes-the-menus-worst-flaw)),
  disabled entries that say why.
- **Understanding a structured space** — equipment slots, a realm's spellbooks, a store's
  stock, the knowledge base. Here a bespoke window earns its keep, because the *structure* is
  the information: the paper doll's body layout
  ([OBS-25](#obs-25--the-items-window-is-a-paper-doll-and-it-is-the-best-thing-in-the-front-end)),
  the Book window's five sortable columns
  ([OBS-40](#obs-40--the-book-window-is-a-table-view-of-data-42-already-computes)).

That is a checkable rule, and it **bounds T7** rather than leaving it open-ended: one
excellent generic pane, plus roughly four bespoke windows. Which is, not coincidentally,
exactly the four the author had finished.

*Corollary:* the author's own release order is a priority list produced by three years of
play-testing. He built stores, then Choice, then Book, then the paper doll. If T7 needs an
order, that is a better one than we would invent.

---

## OBS-44 — `Tool` holds one item, and it disappears. The menu audit is complete
*Observed: "Tool brings up that vault editor."*

Confirmed from source, and it is tidier than expected. `main-window.tcl:872`:

```tcl
if {[file exists [CPathTk vault vault-editor.tcl]]} {
    # Tool Menu
    …
    lappend entries [list -type command -label [mc "Vault Editor"] \
        -identifier E_TOOL_VAULT]
}
```

The **entire `Tool` menu is conditional on `vault-editor.tcl` being present**, and it holds
exactly **one** entry. It was an optional developer tool, shipped as a file you could delete.

Since decision 2 drops all four data editors
([OBS-28](#obs-28--the-alternate-editor-conditional-tile-substitution),
[OBS-35](#obs-35--the-sound-editor-skip-it-but-note-what-the-earlier-decision-did-not-cover)),
**the `Tool` menu ceases to exist** — no work, no decision, it simply goes.

---

## The complete menu-bar audit

All eight menus captured. This table is T7's input.

| Menu | Contents | Disposition |
|---|---|---|
| **File** | Save `^S` · Quit With Save `^X` · Quit | **Keep, renamed and reordered.** `^S`/`^X` map exactly to 4.2. "Quit" ≠ 4.2's `Q` (which *retires* the character) — name both destructive actions for what they do, confirm both, and move Quit/Preferences to the macOS application menu ([OBS-42](#obs-42--the-file-menu-three-entries-one-semantic-trap-and-two-accelerator-namespaces)) |
| **Inven** | Equipment · Inventory · Magic ▸ · Use ▸ · Inspect · Inscribe · Uninscribe | **Keep.** 8 of 10 `Use` entries map straight across; `Destroy`→`Ignore` is a semantic change, `Jam Spike` is gone ([OBS-19](#obs-19--inven--use-and-one-semantic-trap)) |
| **Book** | realm ▸ book ▸ spell, three levels deep | **Keep.** All 24 books across 6 realms already in `class.txt`; `spell_okay_to_cast()` supplies greying ([OBS-38](#obs-38--help-and-book-and-the-philosophy-that-ties-the-whole-front-end-together), [OBS-39](#obs-39--two-realms-in-the-book-menu-and-m9s-structure-turns-out-to-be-done)) |
| **Action** | Alter ▸ · Looking ▸ · Movement ▸ · Resting ▸ · Searching ▸ · Note · Repeat · Target · Pets · Use Power | **Keep** — but six of ten live in 4.2's `cmd_hidden`, so the menu map must promote out of it ([OBS-18](#obs-18--what-note-does-and-why-cmd_hidden-is-a-trap-for-the-menu-generator)) |
| **Other** | Character Info · Feeling · File Character · High Scores · Knowledge · Message History · Photo ▸ · Preferences ▸ · Quest Status · Time Of Day · Use Music/Sound | **Keep, split.** Four already in 4.2 with identical keys; Quest Status and Time Of Day are Zangband-specific (M6, M4); Photo and Preferences are front-end only ([OBS-20](#obs-20--the-other-menu-three-categories-one-delightful-loop-and-one-more-collision)) |
| **Tool** | Vault Editor | **Gone.** |
| **Window** | Arrange · Maximize · Save/Load/AutoSave Positions · six window toggles | **Mostly gone.** Panes make Arrange and Maximize meaningless; keep auto-saved layouts and pane visibility toggles ([OBS-37](#obs-37--the-window-menu-and-arrange-windows-is-the-800px-maths-meeting-a-5k-screen)) |
| **Help** | Help `?` · Tips · About | **Keep**, rendering Sphinx instead of TkHtml ([OBS-08](#obs-08--the-help-system-is-tkhtml-not-winhelp)) |

**Three key collisions** to settle game-side, not in T7: `p` (pets vs start exploring),
`U` (racial powers vs use an item), `^T` (time of day vs 4.2's alternate tunnel binding).

So the menu bar becomes **seven menus**, with `Tool` dropped and `File` reduced by the macOS
application-menu convention. Every leaf either maps to a 4.2 command, is a documented
Zangband addition owned by a Phase 2 milestone, or is a front-end feature with an entry in
this log.

---

## OBS-10 — The reference build is unstable, and some of that is fixable
*Reported: "the game is very unstable, crashing a lot."*

Three separate causes, and two have remedies sitting in the archive.

### 1. There is an official patch, and this install does not have it

[`archive/Tk/ztk240r5fix1.zip`](../../archive/Tk/ztk240r5fix1/) — "ZTk 2.4.0r5 fix 1",
dated 1 October 2001 — replaces two scripts. Both the source tree *and* the archived runtime
are **unpatched**, so a CrossOver install built from either is missing it.

**`misc-window.tcl` — a typo that breaks a documented feature.** The proc is defined at
line 863 as `NSMiscWindow::UpdateHP_SP_FD`. Three call sites spell it correctly. Line 1296,
inside `NSMiscWindow::ToggleProgress`, spells it **`Update_HP_SP_FD`** — an extra
underscore, so the command does not exist and the call raises every time.

`ToggleProgress` is the Progress Window's right-click popup that switches between bars and
numbers — which is to say, one of the 32 advertised tips ("You can change the appearance of
the Progress Window with a popup menu") **has never worked in 2.4.0r5**. Do not chase that
one as a Wine problem.

The same patch also changes how `race` and `class` bind in the Misc window, moving them out
of the generic label loop into `bind_Py_value`.

**`value-manager.tcl` — a missing `return`, and this one is not cosmetic.** In the
`music,directory` / `savefile` branch, when a path lies outside both `Path` and `CPath`, the
unpatched code falls out of the `switch` and hits `return [list $value]`, wrapping the value
in an extra list level. The patch adds `return $value`.

This is the code that **serialises preferences to disk**. A savefile or music directory
outside the game tree — precisely what a CrossOver bottle with a non-standard layout
produces — gets double-wrapped on write and read back corrupt. Plausible cause of
path-related failures that survive restarts.

### 2. The application's own crash advice

The Setup → Music tab says it outright: *"If your computer does not have a sound card, or
the game crashes a few seconds after starting, you should disable music."* Music is BASS
over DirectX 3 ([OBS-03](#obs-03--music-is-bass-and-only-bass)) — the most Wine-fragile
thing in the build. Sound is DirectSound or WaveMix and nearly as fragile.

### 3. Genuine 2001 bugs, from `tk/doc/bugs.html`

- Num Lock turns itself off after long stretches of key-bashing — *"someone said this
  happened when a sound was assigned to the Walk event."*
- `gzclose() failed` during startup while reading an icon data file. **Recovery: delete the
  icon data file and restart** — the game regenerates it.
- 256-colour palette handling is "silly" and gets colours wrong.
- Detection-spell feedback shows even when the device is unidentified.

### To stabilise the reference

1. Copy both files from `archive/Tk/ztk240r5fix1/` over the install's `OmnibandTk-1.4/tk/`
   copies. Drop-in replacements, no merge needed.
2. Setup → Music: **off**.
3. Setup → Sound: **off** to begin with; re-enable later if you want to observe the sound
   system, and never assign a sound to the Walk event.
4. On a `gzclose()` error, delete the icon data file rather than reinstalling.

**No bearing on the port.** All four are 2001 or Wine artefacts. Worth the effort only
because a stable reference is the point of having one at all — and the `ToggleProgress`
typo is a reminder that the original is a *reference*, not an oracle: where it and
`interface.html` disagree, the document may well be right and the build wrong.

---

## Still to capture while the reference runs

Cheap now, expensive later. In rough priority:

1. **The remaining menus — File, Book, Other, Tool, Window, Help.** Highest yield of
   anything on this list, for the reason in
   [OBS-19](#obs-19--inven--use-and-one-semantic-trap): the menu bar is a differential audit
   of command coverage, and it is the direct input to T7's menu map. One screenshot each.
2. **The preference windows**, which exist only as live UI and appear in no document:
   Sound, Assign, Color, Options.
3. **Screen recordings of each `interface.html` rule** — event ordering, what `inkey_flags`
   holds at each prompt, the Recall/Choice hand-off, redraw timing.
4. **The Knowledge, Macros and Keymap windows** — described in prose, never illustrated.
5. **The building/service screens**, which is where `townactions.gif`'s 72 icons are used
   and which Phase 2's M5 has to reproduce.
6. **Birth**, end to end — the one flow deferred to T8 and the one most changed by M7/M9.

---

## OBS-45 — The class-dependent menu bar, the allegiance-coloured health bar, and 4.2's 18 front-end hooks
*Screenshots: a Mindcrafter whose menu bar carries **Mindcraft** in place of **Book** — twelve
powers, flat, `a`–`l`. Farmer Maggot with a **blue** health bar. The **Powers** window:
`Name · Level · Cost · Fail · Stat`, one row — `a) Mind Blast · 15 · 12 · 100% · Intelligence`.*

### 1. The menu bar is built per character

`main-window.tcl:716`:

```tcl
if {[string equal [angband player class] Mindcrafter]} {
    NSModule::LoadIfNeeded NSMindcraftMenu
    NSObject::New NSMindcraftMenu $mbarId
}
```

So `Mindcraft` **replaces** `Book` for that class — flat list, no realm/book levels. The
declarative menu map from
[OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design) therefore
cannot be one static tree; it must be **evaluated against the character**. That matters more
for us than for the original, since M7 adds classes and M9 realms. Note also the hard-coded
class *string* — brittle in exactly the way a data-driven map is not.

### 2. The health bar is coloured by allegiance — and this corrects OBS-29

Farmer Maggot's bar is **blue**, not red. `main-window.tcl:1663` is explicit:

> *"The monster health bar is displayed differently for friendly versus non-friendly
> monsters. This is done by using a different set of colors for each state."*

Four slots — `BarDone`, `BarToDo`, `BarBL`, `BarBD` — each with a colour **and an opacity**,
pre-allocated per state.

**So the Color editor's Monster Bar tab shows hostile health and friendly health, not health
and mana.** [OBS-29](#obs-29--the-color-editor-a-2001-toy-that-is-now-an-accessibility-feature)
is corrected above.

And this lands on a standing Phase 2 constraint three milestones early: *"Do not foreclose
PLR-22. Everything touching monster handling before M10 must leave room for monsters to have
an allegiance."* The health bar must carry allegiance the **first** time it is drawn, in T5,
or it gets retrofitted.

### 3. One ability table, not three windows

The Powers window is the same widget as the Book window with different columns —
`Cost` for `Mana`, `Stat` added, `Comment` dropped — and the same toolbar. With Mindcraft
that is three consumers of one pattern. **Build one parameterised ability table**, and
`power.tcl` (802) + `mindcraft-window.tcl` (818) + the Book window collapse into it.

*Content status:* racial powers are **already implemented in this tree** under PLR-02 —
[player-util.c:2095](../../src/player-util.c#L2095) *"How likely a racial power is to fail
(ZangbandTK, PLR-02)"*, with supporting code in `player-spell.c` and `init.c` and a test in
`main-test.c` that exercises M8. Only the display is missing.

### 4. The finding that reshapes T3: 4.2 has 18 replaceable front-end hooks

Chasing the Powers window's data source led to
[game-input.h:42](../../src/game-input.h#L42), which declares **eighteen function pointers**
the front end may replace:

```c
bool (*get_item_hook)(struct object **choice, const char *pmt, …);
int  (*get_spell_from_book_hook)(struct player *p, const char *verb, …);
int  (*get_spell_hook)(…);
void (*view_abilities_hook)(struct player_ability *ability_list, …);
int  (*get_quantity_hook)(const char *prompt, int max);
bool (*get_aim_dir_hook)(int *dir);
bool (*get_rep_dir_hook)(int *dir, bool allow_none);
bool (*get_point_hook)(struct loc *grid);
bool (*get_check_hook)(const char *prompt);
bool (*get_string_hook)(const char *prompt, char *buf, size_t len);
bool (*get_curse_hook)(…);   int (*get_effect_from_list_hook)(…);
…
```

**This is the seam the original never had, and it changes the bridge's design.**

Plan §3.2 models the bridge as 190 accessor commands with Tcl reading game state — the
original's model, forced on it because `DoUnderlyingCommand` could only synthesise keystrokes
([OBS-43](#obs-43--the-uneven-menus-are-unfinished-work-not-a-design-philosophy--and-the-change-log-proves-it)).
These hooks invert it: **the game calls us when it needs a choice.**

| Screen | Becomes |
|---|---|
| the paper doll / item chooser | `get_item_hook` |
| the Book table | `get_spell_from_book_hook` |
| the Powers / Mindcraft table | `view_abilities_hook`, `get_spell_hook` |
| targeting on the map | `get_point_hook`, `get_aim_dir_hook` |
| store quantity entry | `get_quantity_hook` |
| every confirmation | `get_check_hook` |

Combined with the typed command queue — `cmdq_push()` plus `cmd_set_arg_item()` and friends —
both directions are covered:

- **We initiate:** submit a command with its arguments already bound. No prompt.
- **The game initiates:** it calls our hook, we show a native chooser, we return the answer.

**Neither requires keystroke synthesis or `inkey_flags` polling.** Which is why
[OBS-43](#obs-43--the-uneven-menus-are-unfinished-work-not-a-design-philosophy--and-the-change-log-proves-it)'s
unevenness stops being a per-command engineering problem: it is not "finish converting the
commands", it is "install the right hooks once".

**Action: T3 needs reworking around hooks rather than accessors, and the bridge estimate
almost certainly falls.** Many of the 190 commands existed to work around the absence of
these seams. That is a substantial revision and it should be done deliberately, not as a
footnote — it is the top item for the next session on this plan.

---

## OBS-46 — The death flow, end to end: a memorial system, and one thing not to copy
*Screenshots: **Tomb** — RIP / hgfd the Trainee / Mindcrafter / Level 1 / Exp 0 / AU 160 /
"Killed on Level 0 by a Wild cat", the character's **tile icon** below it, and buttons
Messages · File · Continue. Then **Character Record** — "Choose the information you wish to
be kept for your character. The information can be viewed in the High Scores Window", with
checkboxes **Character dump · Message log · Photo**. Then the **Hall of Fame** with the new
entry boxed in cyan and "No record available." in the status bar.*

Four steps: **Tomb → Character Record → Hall of Fame → exit.**

### The Character Record dialog answers a question left open in OBS-23

[OBS-23](#obs-23--the-hall-of-fame-is-a-native-window-with-two-features-worth-keeping) noted
"No record available." in the Hall of Fame's status bar and guessed at a per-entry detail
pane. Confirmed, and better than guessed: a score entry can carry an **attached record** —
the character dump, the message log, and a **photo**. The status bar reads "No record
available" here precisely because those boxes were left unticked.

So this is a **memorial system**: your last screenshot, your final messages and your sheet,
kept against your name in the Hall of Fame. It also explains why Photo captures cave data and
stays inspectable ([OBS-20](#obs-20--the-other-menu-three-categories-one-delightful-loop-and-one-more-collision))
— it was designed to be *kept*, not glanced at.

**4.2 has no equivalent.** `struct high_score` carries `what, pts, gold, turns, day, who,
uid, p_r, p_c, cur_lev, cur_dun, max_lev, max_dun, how` and nothing else — no attachment
concept. So this joins the six unique tile sets as a genuine original-only feature.

**Worth building, in T9.** Every part already exists for other reasons: 4.2 produces
character dumps and message logs, and Photo is being built anyway for the tileset preview.
The marginal cost is an attachment store keyed by score entry, plus the Hall of Fame detail
pane. It is the kind of small thing that makes people fond of a game.

### The tombstone art is already in the tree, as data

[lib/screens/](../../lib/screens/) carries **`dead.txt`** — the RIP tombstone as ASCII art —
plus **`crown.txt`** (the winner's screen, i.e. `tomb.tcl`'s `KinglyWindow` equivalent) and
**`retire.txt`**. So the art is template-driven and needs no porting.

The one thing the original *adds* is the **character's tile icon on the tombstone**, which
the ASCII template cannot have. Keep that: it costs one icon draw and it is the difference
between a generic tombstone and *your* character's.

### The one thing not to copy: exiting

*"Close the window and the game exits… hmmmm"* — the hmmm is right. On macOS an application
that quits because your character died is simply wrong, and the fix is already sitting in
4.2.

`ui-death.c:356`'s death menu has **nine** actions where the original has three:

```
i Information · m Messages · f File dump · v View scores
x Examine items · h History · s Spoilers · n New Game · q Quit
```

Note **`n New Game`**, and a comment requiring Quit to stay last. So 4.2 already expects the
player to browse the corpse and then **roll a new character without relaunching**.

And we have somewhere to land: the pre-game launcher window exists with all game data loaded
before `play_game()` is called ([OBS-01](#obs-01--a-startup-launcher-runs-before-the-game-exists)).
So death returns to the launcher, and only Quit quits.

**The flow becomes:** tombstone (canvas-styled classic, with the icon) → the nine-action menu
as native UI, since this is the "structured space" case
([OBS-43](#obs-43--the-uneven-menus-are-unfinished-work-not-a-design-philosophy--and-the-change-log-proves-it))
→ optional Character Record → Hall of Fame → **New Game returns to the launcher**.

Better than the original and better than the term build, and every piece of it already
exists.

---

## OBS-47 — The Vampire starts at night, and night is a tile-lighting requirement
*Screenshot: "bob", a **Vampire Warrior-Mage**, with the message line reading "This is day 1.
The time is 12:12 AM. It is deep night." The Outpost is mostly black, with stone lit only
near the player and the grass and trees rendered in visibly **darkened** tiles.*

The gameplay detail is a nice touch — a Vampire is created at night, since sunlight is a
problem for it. That belongs to M4 (day/night) and M7 (races), and it is the same system as
`Time Of Day Ctrl+T` in the Other menu
([OBS-20](#obs-20--the-other-menu-three-categories-one-delightful-loop-and-one-more-collision)),
which has no 4.2 equivalent.

**But the Phase 3 consequence is the tile lighting**, and it is a T2 requirement I had not
recorded: night makes this **wilderness** dark, not just a dungeon. Every terrain tile needs
a darkened rendering.

The 2.4.0r5 changelog says how the original did it, and it is the memory-efficient answer it
arrived at only in its final release:

> Icon lighting no longer requires 3 copies of each icon in a GIF file. Instead, 2 darkened
> copies of an icon are created only when needed. The Tcl command
> `icon dark $type $index $gamma1 $gamma2` is used.

So: **two darkened variants, generated at runtime by gamma**, on demand rather than shipped
in the sheet. The `tk/config/dark` file — a bare list of palette indices
(`246 44 45 46 47 47 49 50 51 …`) — is the accompanying palette remap.

**4.2 supplies the states, not the renderings.** [cave.h:139](../../src/cave.h#L139) defines
four per grid — `LIGHTING_LOS`, `LIGHTING_TORCH`, `LIGHTING_LIT`, `LIGHTING_DARK` — chosen in
[cave-map.c:114](../../src/cave-map.c#L114), and the term front end handles them by shifting
the *colour attribute* ([ui-map.c:171](../../src/ui-map.c#L171):
`get_color(*a, ATTR_LIGHT, 1)`). That works for a letter; it does not work for a tile.

**So T2's icon layer needs a lighting dimension**: map 4.2's four states onto rendered
variants, generating darkened copies by gamma on demand as the original finally did. Together
with the animation finding ([OBS-36](#obs-36--the-sprite-editor-closes-the-animation-gap-and-the-spec-is-three-fields))
and composable modes (§6 decision 5), that is the **third** thing that changes the shape of
the tile lookup — from "index → tile" to "index + lighting + frame → rendering". All three
must land in T2's design together, not one at a time.

---

## OBS-48 — The store window, and the accelerator principle generalises to items
*Screenshots: "The Weapon Smiths owned by **Ellefris the Paladin**" — toolbar with `Buy ▾`
and `Sell ▾`, a field row `Quantity | Total Cost | Purse: 10000 | Gold Remaining: 100`, two
shelved panes with per-pane status bars. On hover: an **orange** outline on the shop item, a
tooltip reading **"g) a Katana (3d4) (+0,+0)"**, `Quantity: 1`, `Total Cost: 984`, and the
item's weight appearing in the status bar.*

### One hover, four pieces of feedback

Outline, tooltip, live total cost, and weight — from a single gesture, with no click. And
**two outline colours carrying two meanings**: orange for the hovered candidate in the shop
pane, green for the selected item in the inventory pane. That is a small thing done properly;
hover-versus-selection is ambiguous in most list UIs, and two colours settle it.

The `Quantity` field with a **live `Total Cost`** is the `get_quantity_hook` case from
[OBS-45](#obs-45--the-class-dependent-menu-bar-the-allegiance-coloured-health-bar-and-42s-18-front-end-hooks)
done well: an inline field you type into and watch, rather than a modal prompt answered
blind. And showing the shop's purse beside your own gold answers "can this shop even afford
my loot?" without attempting a transaction.

### The tooltip carries the item's letter, and that generalises a principle

**"g) a Katana"** — the tooltip includes the keyboard selector. So
[OBS-17](#obs-17--the-menus-show-their-accelerators-and-that-is-the-whole-design)'s rule is
not about menus at all; it is about *everything selectable*:

> Every selectable thing displays the key that selects it.

Menus show accelerators, item lists show letters, spell tables show letters
([OBS-40](#obs-40--the-book-window-is-a-table-view-of-data-42-already-computes)). A
mouse-driven player is being taught the keyboard continuously, in every list, whether they
notice or not. That is a stronger and simpler statement of the front end's thesis than the
menu-only version, and it should be the rule for T7: **no selectable element without its
selector shown.**

### The half that must adapt: 4.2 disables selling by default

4.2 keeps the purse — [store.c:239](../../src/store.c#L239) parses it per owner and
[store.c:669](../../src/store.c#L669) caps what a shop will pay — but **`birth_no_selling`
defaults to `true`** ([list-options.h:110](../../src/list-options.h#L110)), *"Increase gold
drops but disable selling"*.

So with the default option set, `Sell ▾` and `Purse` are meaningless. **Drop them, do not
grey them:** this is not a transient "you cannot sell right now", it is a permanent property
of the game chosen at birth, and a control for something the ruleset does not permit is the
dead-control problem `interface.html` admitted to.

*Also confirmed:* the shelves are `shelf.gif`, the 276×17 strip flagged in
[§1.3](phase3-tcl-tk-frontend.md) as the asset nobody would recreate and nobody would miss
until the stores looked wrong. And the empty third shelf is drawn even with no stock on it —
[OBS-38](#obs-38--help-and-book-and-the-philosophy-that-ties-the-whole-front-end-together)'s
rule once more.

---

## OBS-49 — The buildings, at last — and they are the best-executed screens in the app
*Screenshots: **"The Mayor run by Uldrik the Human"** — `q) Request quest`, `ESC) Exit
building`; then the pane replaced by term-styled text, "Quest information (Danger level: 5)
/ Thieves Hideout / There are thieves robbing my people!…". And **"The White Horse Inn run
by Otick the Human"** — `r) Rest for the night · Cost: 20 gp`, `f) buy Food and drink ·
**CLOSED**` (visibly dimmed), `w) Listen for rumors · Cost: 5 gp`, `q) Request Quest`, with
`ESC) Exit building` in a second column.*

The last blind spot, and the one with no documentation and no substitute. **4.2 has no
buildings at all**, so M5 invents this system and these two screenshots are now its only
visual reference.

### The shape is generic, which is what M5 needs

A building is **(name, owner name, owner race, list of services)**. A service is
**(icon, accelerator, label, cost-or-status, handler)**. One window renders them all:

- a serif display title
- a **two-column** grid of wide action bars, with `Exit` given its own column
- `Gold Remaining` pinned in the header, because services cost money
- **cost shown inline** on each action — `Cost: 20 gp` as a second label line
- selecting a service **replaces the pane** with term-styled result text
  ([OBS-15](#obs-15--the-classic-character-sheet-is-not-unfinished--it-is-a-canvas-on-purpose)'s
  middle tier again), as with the quest description and its **Danger level: 5**

So M5 should define buildings as **data** — a table of buildings, each with a service list —
and the front end renders one window for all of them. That matches how Zangband's `bldg.c`
works and it means adding a building is a data change.

**`townactions.gif`'s 72 icons are confirmed as the service icons** — a bed for Rest, food
and wine for the tavern, a RUMORS placard, crossed swords and skull for Request Quest, a door
for Exit. That asset finally has its owner, and the count tells us roughly how many distinct
services Zangband's buildings offered.

### And they show disabled state properly — which corrects an over-generalisation of mine

`f) buy Food and drink` reads **CLOSED** and the whole bar is **dimmed**.

I had built a running theme that the original "knows something and fails to show it" —
the Flags table's grey wall, the unexplained empty Notes tab, the Book menu's invisible
disabled state ([OBS-41](#obs-41--the-recall-window-is-a-universal-detail-pane-and-it-fixes-the-menus-worst-flaw)).
**The buildings do it correctly**: dimmed *and* labelled with the reason.

So the correct reading is not "the original never shows disabled state" — it is that the app
is **inconsistent**, which is another symptom of it being unfinished rather than of a design
philosophy. And usefully, **the fix I proposed is the original's own best practice**: dim it
and say why. Buildings are the standard the Book menu and the Flags table should have met.

That also makes buildings the best-executed screens in the application: every service carries
its icon, its accelerator, its cost, and its availability with a reason. Nothing else in the
app manages all four.

**For T7 and M5:** copy this screen almost exactly. It is the one place where "copy the
affordances, not the layouts" collapses into "copy both".

---

## OBS-50 — The Home is the store window parameterised, and that completes the surface count
*Screenshot: "Your Home" — menu `Home`, toolbar `Take ▾` (**dimmed**) and `Drop ▾`, a
`Quantity` field, `Gold Remaining: 100`, two shelved panes reading "Your Home · 0 items" and
"Inventory · 5 items · 27.0 lb".*

Identical to the store window with three parameters changed:

| | Store | Home |
|---|---|---|
| verbs | Buy / Sell | **Take / Drop** |
| money | `Total Cost`, `Purse` | **absent** — no transaction |
| owner | "owned by Ellefris the Paladin" | **none** — it is yours |

Everything else is the same widget: shelves, two panes, quantity field, per-pane status bars,
selection outlines.

**And `Take` is dimmed, because the home holds 0 items.** That is the *third* place the
original handles disabled state correctly, after the Inn's `CLOSED` and its dimmed bar
([OBS-49](#obs-49--the-buildings-at-last--and-they-are-the-best-executed-screens-in-the-app)).
The pattern is now clear: **the original does disabled state well in transaction windows and
badly in menus and tables.** Inconsistent, not absent — which is the same "unfinished"
diagnosis as everywhere else.

### The surface count: T7 is five reusable widgets, not twenty screens

With the Home, every screen in the application has now been seen, and they collapse into a
short list:

| Surface | Serves | Parameterised by |
|---|---|---|
| **Transfer window** | Store, Home | verbs, money on/off, owner |
| **Building window** | Mayor, Inn, and the rest | name, owner, service list |
| **Ability table** | Book, Powers, Mindcraft | columns, source, state predicate |
| **Generic selection pane** | every "choose an item" prompt | item tester, mode |
| **Paper doll** | Equipment | body type |

Plus the map pane with its four overlay planes, the universal detail pane, and the Character
window's five tabs.

**That is a much smaller T7 than "convert every command to a dialog."** It also matches
[OBS-43](#obs-43--the-uneven-menus-are-unfinished-work-not-a-design-philosophy--and-the-change-log-proves-it)'s
test exactly: each of these five exists because *structure is the information* — two
inventories facing each other, a list of services with costs, spells with levels and fail
rates, equipment slots on a body. Everything else is a filtered list, and the generic pane
handles it.

Five widgets, each parameterised, each mapping onto a 4.2 hook or command. That is the
milestone's real scope.

---

## OBS-51 — The Pets window is real and reachable (correcting OBS-50's note)
*Screenshots: `p` puts "(Command a-g, *=List, ESC=exit) Select a command:" on the message
line and lists seven pet commands in the **Choice** pane, with the current stance
(`b) Follow me`) in cyan. Separately, a **Pets** window: menus `Pets` and `Command`, the
Book/Powers toolbar, a table of **Name · Upkeep · Status**, and a status bar reading
**"0 pets  Upkeep: 0%"**.*

**Correction.** I claimed the Pets window was never wired to anything, inferring that from a
grep that found only the module loader. That was not sufficient evidence and the claim was
wrong: the window exists, opens, and carries its own **`Command` menu**.

So there are **two routes to the same commands**, which is the *good* pattern rather than
the unfinished one:

- **`p`** — the quick keyboard path, served by the generic selection pane.
- **The Pets window** — the roster, with `Command` for the same seven actions.

That is quick-access plus full-view, which is what a well-built app offers.

### What is still worth taking for M10

- **Three columns: Name · Upkeep · Status.** `Upkeep` is load-bearing, not decorative —
  pets drain experience in Zangband — and the status bar aggregates it: **"0 pets ·
  Upkeep: 0%"**. A player needs the running total more than the per-pet figure, and the
  original puts it exactly where it belongs.
- **The window's `Command` menu already gets the shape right** — and my critique was aimed at
  the wrong half. It has a **separator** dividing the five mutually exclusive *stances* (Stay
  Close · Follow Me · Seek And Destroy · Give Me Space · Stay Away) from the two independent
  *toggles* (Open Doors · Pick Up Items), a **radio check** on the current stance, and a
  **checkbox** on the active toggle, with `a`–`g` shown as accelerators. That is exactly
  right. It is the **`p` prompt / Choice pane** version that is lossy — a flat lettered list
  that cannot show which toggles are on. Two routes, one good, one a stopgap: the same
  pattern as everywhere else in this application.

- **The real defect is discoverability, and it is worse than the shape problem.** The Pets
  window opens only via `p` then `*` — and `*` is discoverable *only* from the prompt text
  "(Command a-g, `*`=List, ESC=exit)". No Window-menu entry, no toolbar button, nothing in
  the Action menu beyond `Pets p`. A 1,231-line window with a roster and an upkeep total,
  reachable by knowing to press an asterisk.

  **T7 requirement: every window has a discoverable route, not just a keystroke sequence.**
  The Window menu lists every pane and window that exists, and a command with a window behind
  it says so. One menu entry per window, and it is the difference between a feature existing
  and a feature being used.

  The cost of getting this wrong is measurable here: a full day's systematic tour of this
  application, and the Pets window was the **last** thing found — because nothing in the
  interface said it was there.
- **The toolbar is the Book/Powers toolbar again**, which refines
  [OBS-50](#obs-50--the-home-is-the-store-window-parameterised-and-that-completes-the-surface-count):
  what I called an "ability table" is really a **generic table widget** — columns, a source,
  and a per-row state predicate. Its consumers are Book, Powers, Mindcraft, **Pets**, and
  arguably Knowledge and the Hall of Fame. That tightens the surface count rather than
  loosening it.

---

## Conclusions from the reference build

### The dates confirm it: the author stopped in October 2001

| Date | What |
|---|---|
| 4 Aug 2001 | ZAngbandTk **2.4.0r5** — the last release (per `changes.html`) |
| 5 Oct 2001 | `zangtk-240r5-src`, `-win32`, and **`ztk240r5fix1`** — a two-file bugfix |
| 24 Oct 2001 | `image-1.4`, `AngbandTk_install` — **last first-party artefact** |
| 30 Jan 2002 | `omnibandtk_vc_projects` — third-party MSVC project files |
| 11–15 Feb 2002 | `squelch_patch` — third-party feature patch |
| 17 Dec 2002 | `JLEAngbandTk-292r2` — a fork |

Tim Baker's work ends **October 2001**. The community patched on for another fourteen months,
then it stopped. So yes: 2.4.0r5 is the last version, and the last thing the author shipped
was a bugfix, not a feature.

### But "incomplete" needs splitting, because the two halves differ sharply

**The implementation is incomplete**, and the change log shows exactly how: commands were
being converted from keystroke-synthesis to bespoke windows **one per release for three
years** ([OBS-43](#obs-43--the-uneven-menus-are-unfinished-work-not-a-design-philosophy--and-the-change-log-proves-it)).
Stores, then Choice, then Book, then the paper doll. Eat Food was still in the queue when
work stopped. And the loose ends are visible: a typo that has broken the Progress Window's
context menu since release ([OBS-10](#obs-10--the-reference-build-is-unstable-and-some-of-that-is-fixable)),
a photo `Open` path that could not read the formats it offered
([OBS-26](#obs-26--photo-confirmed-open-crashes-and-tk-9-absorbs-all-three-dead-extensions)),
an options window with controls that did nothing, four known bugs never fixed.

**The design is substantially complete, and coherent.** That is the part worth being precise
about, because it is what we actually inherit:

- **A written interaction specification** — `interface.html`'s ten mouse rules and
  per-window behaviour, plus 32 tips ([OBS-07](#obs-07--the-interaction-model-was-written-down)).
- **Four fully-realised bespoke windows** that are genuinely good: the paper doll, the Book
  table, stores, knowledge.
- **Idioms applied consistently across the whole app**, which is the mark of a design rather
  than an accretion: one universal detail pane fed by 25 of 69 scripts
  ([OBS-41](#obs-41--the-recall-window-is-a-universal-detail-pane-and-it-fixes-the-menus-worst-flaw));
  show the whole space and mark your position in it
  ([OBS-38](#obs-38--help-and-book-and-the-philosophy-that-ties-the-whole-front-end-together));
  preview through the real renderer, never a mock-up
  ([OBS-29](#obs-29--the-color-editor-a-2001-toy-that-is-now-an-accessibility-feature)).
- **Three years of play-tested priority ordering**, in the release sequence.

### So we are not finishing his program — we are implementing his design without his constraint

The reason the implementation stalled is now known precisely. `DoUnderlyingCommand` feeds
**synthesised keystrokes** into the game, 88 call sites, guarded by `inkey_flags` because the
game might not be accepting a command. Every bespoke window therefore had to drive not just
one keystroke but the whole prompt sequence behind it — which is why each conversion cost a
release.

4.2 has a **typed command queue** — `cmdq_push()` plus `cmd_set_arg_item()`,
`cmd_set_arg_direction()`, `cmd_set_arg_target()`, `cmd_set_arg_point()`,
`cmd_set_arg_number()`. So a chooser can gather the arguments and submit the command with
them already bound. No keystroke synthesis, no prompt round-trip.

**That is the single largest thing the port gains, and it retires the reason the original was
uneven.** "Complete" versus "incomplete" menu items stops being per-command engineering and
becomes a property of the architecture. Bespoke windows then earn their place only where the
*structure* is the information, which is a much smaller list
([OBS-43](#obs-43--the-uneven-menus-are-unfinished-work-not-a-design-philosophy--and-the-change-log-proves-it)).

### What that means for the estimate

The audit changed the shape of Phase 3 rather than its size:

- **Removed:** four data editors (~9,000 lines), `savefile-z.c`, `notes.c`, the birth state
  machine, the monster grouping, the command and options tables, three dead binary
  extensions, the multi-variant launcher, all seven sound backends.
- **Added:** a third `plat` backend for Aqua, animated-tile support in the icon layer, a
  colour-blind-safe palette, six tile sets 4.2 lacks, and the argument-driven dispatch that
  makes the rest cheaper.
- **Unchanged and still the bulk:** the bridge rewrite, now better bounded because
  §6 decision 2 and the audit in [OBS-44](#the-complete-menu-bar-audit) say what it must
  cover.

The honest summary: **more work than the original did, less work than the original's line
count implies, and with a clear specification for all of it** — which is not the position
most twenty-five-year-old resurrections start from.
