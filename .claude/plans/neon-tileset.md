# The Neon tileset, the shimmer, and the palette storm

**Status:** tileset and shimmer **shipped on master** · palette storm **parked** ·
**Parked:** 6 September 2026

Parked mid-thought after a sidetrack, so this is written to be picked up cold. §1–§3 are
what exists and needs nothing. §4 onward is the unfinished idea and the decisions it is
waiting on.

Designs and mockups: [`archive/scratchpad/`](../../archive/scratchpad/).

---

## 1. What shipped

All of it is on `master`, in this order:

| | |
|---|---|
| `8a9571ed7` | Every monster in the game has a tile, and not one is borrowed |
| `a42b77221` | Colour cycling — abandoned, kept for the record |
| `b655c3fac` | The floor was there all along, at two to one against black |
| `f32813634` | Merge of the cycling branch |
| `2d5c5e9f1` | Gold shimmers at last, and silver is not a rainbow |

The `neon-tileset` and `neon-colour-cycling` branches are **stale**. Their content is on
master under different SHAs after the rebase, and `git diff master..neon-tileset` shows
the branch would now *remove* borg work. Delete both; do not merge them.

---

## 2. The tileset

Graphics **mode 7**, named "Neon tiles" in [`lib/tiles/list.txt`](../../lib/tiles/list.txt).
Modes 5 and 6 stay retired with Shockbolt's set.

**105 silhouettes → 2092 preference lines.** Complete coverage: 1013 monsters, all 42
terrain features, 157 objects, 302 flavours, 152 trap lines, 300 spell-effect lines. It
needs no `graf-ztk.prf` — nothing is borrowed because nothing is missing.

The idea is that **colour is the row and shape is the column**, so the same drawing exists
in every colour and both halves of a preference line are read out of `lib/gamedata`. Every
other tileset paints colour into its art, and the game then discards its own: once a
graphic tile is assigned, [`ui-map.c`](../../src/ui-map.c) short-circuits on `da & 0x80`
and `color:` never reaches the screen.

### Working on it

The art is **text** — `scripts/tiles/neon/shapes/*.txt`, sixteen lines of sixteen
characters, `X` for ink. The sheet is a build artifact; **never edit the PNG**.

    scripts/tiles/build-neon.py            build the sheet and the prf
    scripts/tiles/build-neon.py --check    measure the palette and the output
    scripts/tiles/build-neon.py --preview  a mock map, at 3x and at actual size
    scripts/tiles/build-neon.py --cycle    a filmstrip of the shimmer

Adding a shape is two edits: draw it, then name it in `manifest.txt` against a `base:`, a
`feat:` code, a `tval:`, a `trap:` code or a `gf:` direction. Anything unnamed has no tile
and renders as its ASCII letter, which is the intended state of an unfinished set.

`--check` is not decoration. Four defects shipped or nearly shipped during this work and
every one was silent; each now has a check that fails instead:

* `lookup_sval()` compares against `obj_desc_name_format()` of a kind's name, so
  `& Small wooden chest~` must be written `Small wooden chest`. All 157 object lines
  resolved to nothing until the formatter was ported into the script.
* Colour code `T` (Light Teal) had no mapping, costing one potion flavour.
* Colour names are spelled four ways across the data files — `Light Green`, `Light dark`,
  `L_DARK`, `ORANGE` — so they are folded, not matched.
* `trap.txt` gives most colours as single letters and five as full names, so a code-only
  resolver dropped those five without a word.

The cheapest check is the one that caught the last: **every shape named in the manifest
must end up in the sheet.** A shape drawn and never used means its lines went nowhere.

### Two lessons worth not relearning

**Judge at actual size.** The floor, road and lava shipped invisible at 1.7:1 to 2.0:1
against black. It passed review at 3× zoom, where a 2:1 colour is a three-pixel blob and
at 1× is nothing. `--preview` now writes `preview-actual-size.png`; read that one.

**A feature's tone is a target, not a nudge.** Relative shifts stack with the lighting
offset and clamp, so four ground features starting from four base tones all landed on
`shadow` together. The fourth field of a `feat:` line is now the tone it sits at when lit.
One rule falls out: **lit terrain never sits at `pale`** — that is the top of the ramp and
what a torch does, and Angband's light colours all map there.

---

## 3. The shimmer, as it now stands

Three things Angband's own implementation never did:

| | Angband | now |
|---|---|---|
| Monsters shimmer in text | yes | yes |
| Monsters shimmer in tiles | never | yes, on a set declaring `cycle:` |
| Items shimmer | never, despite the docs saying so | all 11 treasures, both modes |
| `ATTR_RAND` colour in tiles | discarded | preserved |

Mechanically: a tileset opts in with `cycle:<hues>:<tones>:<span>` in `list.txt` (Neon
declares `10:4:9`). Monsters rotate **hue** via `graf_cycle_attr()`; treasure moves
**tone** via `graf_glint_attr()`, because silver is not a rainbow and grey sits outside
the span, so hue rotation left silver, opals and diamonds completely still.

In text mode this needed **no new data at all** — `visuals.txt` has carried cycles keyed
by colour the whole time, and `flicker:y` is gold, `flicker:u` copper.

Two things to know before testing it:

1. **`animate_flicker` defaults to false** and gates every shimmer in the game, in all
   modes. Options → Interface → "Color: Shimmer multi-colored things".
2. **Test with treasure, not monsters.** Of 1013 monsters only 95 shimmer, median depth
   68, two at L10 or shallower, none in town. Three rounds of "it isn't working" came from
   nothing being on screen. Treasure drops from L1.

`mon->attr` is **not** usable as a per-frame carrier: `grid_data_as_text()` writes the
drawing attr back into that field on every redraw, so by the second frame it holds a tile
row. That is what `animation_frame()` exists for.

Tests: `src/tests/ui/grafmode.c` (5) and `src/tests/ui/shimmer.c` (6).

---

## 4. Parked: the palette storm

**The actual creative target**, and what the cycling work above was a poor substitute for.
The reference is Jeff Minter's *Neon* — <http://minotaurproject.co.uk/neon.php> — a
dynamic, trippy explosion of colour. The shimmer is **per object**; Neon is the **whole
field moving at once**. Wrong shape, which is why it never felt right.

### It is possible, and cheaper than the shimmer was

* **Text mode:** mutate `angband_color_table` per tick. It is already mutable — there is a
  prf `color:` directive that writes it — and the Cocoa front end resolves RGB from it
  every time it draws a run of glyphs (`set_color_for_index`). Change the table, force a
  redraw, and the whole screen recolours. This is real palette animation, the 1982
  technique, and the same one Minter was using.
* **Tiles:** add one offset to every tile's row. Only possible on Neon; the four inherited
  sets cannot do it at all.

### Three parameters, not two features

    phase = turn + (x + y*2) / span * wave

| | |
|---|---|
| **speed** | how far round the wheel per tick |
| **wave** | `0` = whole screen shifts as one; higher = a band travels across the map |
| **bloom** | how much colour is forced into things that have none |

`wave = 0` is the flat version and anything above it is the travelling one, so **"do both"
costs nothing** — it is one knob, which suggests exposing it as an *intensity* rather than
a switch. A Chaos effect could start flat and low and wind up as it takes hold.

**Bloom is not optional.** Rotating hue alone leaves greys where they are — hue means
nothing without saturation — so a dungeon of slate walls sits still while the monsters
move, and the effect collapses.

### Mockups

In [`archive/scratchpad/`](../../archive/scratchpad/), rendered from the real palette, the
real Neon sheet, and the game's own 8×8 font read out of `lib/fonts/8x8x.fon`:

| | |
|---|---|
| `palette-text.png` / `palette-tiles.png` | flat, six frames each |
| `palette-text-wave.png` / `palette-tiles-wave.png` | travelling, six frames each |
| `palette-text-wave.gif` / `palette-tiles-wave.gif` | animated |
| `palette-mock.py`, `palette-gif.py`, `gifwrite.py` | what made them |

**Verdict on the mockups: both liked, wave preferred.**

### What is impossible, regardless of effort

Structural limits of a character-cell renderer, not difficulty:

* **No trails, no decay, no optical bloom.** Cells are drawn opaque with no persistence and
  no compositing. Much of what *Neon* actually is lives here.
* **No sub-cell motion.** Everything is locked to the character grid.
* **No smooth gradients.** 28 colours in text, 40 rows in tiles. Bands, not washes.
* **Nothing audio-reactive.** There is no audio analysis in the tree.

You can have a moving colour field. You cannot have a light synthesiser.

---

## 5. The decisions it is waiting on

Both are taste, not engineering, and neither has been made:

1. **Where it lives.** Suggestion was **hallucination**: `g->hallucinate` already exists in
   `grid_data_as_text()` and already makes what you see unreliable, so it costs nothing
   during normal play and the Chaos realm has plenty of ways to induce it. Title and death
   screens are the other obvious homes. But it may be wanted as a plain toggle instead,
   since it is the look the game is aiming at generally.
2. **Both modes, or tiles only.** The text version is the more faithful technique and works
   on every front end; the tiles version only works on Neon.

**The cost that forces the question:** colour is information in this game — white is cold,
red burns, and the player reads colour before shape. A palette storm destroys that while it
runs, so it cannot be the default play state.

---

## 6. The other four tilesets — closed since

Audited and fixed on 10 September 2026, in `52f7ecf61` and `734013370`.  All
four inherited sets now have complete terrain, objects, flavours, traps and
spell effects; every monster still without a tile is a unique, which is the
policy.  `scripts/tiles/audit-tiles.py` is the guard and exits non-zero on a
gap.  See `scripts/tiles/README` — the four silent failure modes listed there
are worth reading before touching any of this.

---

## 7. Outstanding, elsewhere

* **Monsters sharing a `base:` and a `color:` are the same tile.** Oberon, Dworkin and
  Mandor are all violet people, so all three are one violet figure. Per-name shapes and a
  manifest override are the fix, and the named characters are where to start.
* **`shapes/bolt.txt` is the crossbow bolt and `shapes/bolt_0.txt`…`bolt_135.txt` are the
  spell bolts.** Each file's comment says which, but it will trip someone. Fold a rename
  into whatever next touches the shapes.
* **`gifwrite.py` uses the uncompressed GIF form.** The first variable-width LZW encoder
  produced a structurally perfect file that decoded to solid black — the classic
  encoder/decoder disagreement about when the code width grows. Fine for mockups; fix the
  compressor if these ever ship anywhere.
* **`archive/ROADMAP.md` is stale on this.** Its "Follow up Ideas" section still proposes
  the Minter palette as unbuilt, and links the Urizen one-bit tileset as "a nice zx
  spectrum vibe" — which is the one-bit-silhouette approach the Neon set arrived at
  independently. PETSCII and wilderness map fragments in that section are still genuinely
  open.
