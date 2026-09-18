# Handoff: Zangband Character Sheet (Tk/Tcl window redesign)

## Overview
Redesign of the "Character" window in a Zangband (Angband-family roguelike) client currently
implemented in Tk/Tcl. The original is a plain two-column grid of `label: value` pairs in three
grey frames plus a monospace history box. The redesign keeps exactly the same data but presents it
as an **editorial character ledger**: an identity header, three hairline meters for the live
resources, three ruled stat columns, a monospace history panel, and a monospace keybinding strip.

Target: the same single window in the game client. No new data, no new game state — this is a
presentation-layer change only.

## About the Design Files
The files in this bundle are **design references created in HTML** — a prototype showing the
intended look and behaviour, not production code to copy. The task is to **recreate this design in
the target environment**: Tk/Tcl widgets in the existing client (fonts, frames, canvas rules), or
whatever toolkit the client is being ported to. Where a Tk widget cannot reproduce something
(letter-spacing, `font-feature-settings`), approximate it — see "Tk implementation notes".

## Fidelity
**High-fidelity.** Colours, type sizes, spacing and rules are final and taken from the bound
"Classical" design system (`classical-tokens.css` in this folder). Reproduce values as specified.
The only non-final elements are the two derived labels flagged under "Copy decisions".

## Screens / Views

### Character sheet window (single view, no navigation)
**Purpose**: the player reviews identity, progression, combat numbers, physical attributes and
background history; then returns to the dungeon.

**Layout** (outermost → in). All px values below are the literal rendered values; the design system's
spacing scale is `--space-1: 4.6 / -2: 9.2 / -3: 13.8 / -4: 18.4 / -6: 27.6 / -8: 36.8` px.

1. **Desk ground** — full window, background `#eae7e7` (`--color-neutral-200`), padding 27.6px.
2. **Sheet card** — `max-width: 1080px`, centred, background `#f3f2f2` (`--color-bg`),
   1px border `#bab6b6` (`--color-neutral-400`), radius 7px,
   shadow `0 12px 32px rgba(45,43,43,.22)`, `overflow: hidden`.
3. **Title strip** — flex row, padding 9.2px / 18.4px, background `#f8f4f4`
   (`--color-neutral-100`), 1px bottom divider. Left: `ZANGBAND`, Courier Prime 12px,
   letter-spacing .22em, uppercase, `#7d7979`. Centre: 1px flexible hairline `--color-divider`.
   Right: `CHARACTER`, Cormorant Garamond 14px, letter-spacing .16em, uppercase, `#7d5411`
   (`--color-accent-700`).
4. **Identity header** — flex row, wraps, gap 27.6px, padding `27.6 27.6 18.4`.
   - **Sigil**: 84×84 box, 1px border `#b68235` (`--color-accent`), radius 4px, background
     `#fff3e4` (`--color-accent-100`), centred glyph `@` in Courier Prime 46px `#7d5411`.
   - **Names block**: kicker `ROOKIE` (Courier Prime 11px, .2em, uppercase, `#7d7979`);
     `Steven` as `h1` Cormorant Garamond **400** 46px, line-height 1.05, `#201f1d`;
     subhead `Human Warrior of the first depth` Lora *italic* 17px `#605d5d`.
   - **Display figures** (right, flex row gap 27.6px, right-aligned): `LEVEL` / `1` and `GOLD` /
     `134`. Labels Courier Prime 10px .2em uppercase `#7d7979`; figures Cormorant Garamond **300**
     62px, line-height .95, tabular figures. Level figure is `#7d5411`, gold figure `#201f1d`.
5. **Meter row** — CSS grid, `repeat(auto-fit, minmax(220px, 1fr))`, gap 27.6px,
   padding `0 27.6 18.4`. Three meters: Hit points, Mana, Next level in. Each is a label row
   (Courier Prime 11px, .18em, uppercase, `#605d5d`) with the value pushed right in `#201f1d`
   tabular figures, above a **7px-tall track**: 1px border, radius 1px, background `--color-bg`.
   - Filled state (HP 20/20): border `#b68235`, inner fill width = ratio, background `#e1ad66`
     (`--color-accent-400`).
   - Empty/none state (Mana 0/0, XP 0/10): border `#bab6b6`, no fill.
   - Rule: a resource whose maximum is 0 renders as an empty grey-bordered track (never a
     divide-by-zero or a full bar).
6. **Stat columns** — CSS grid, `repeat(auto-fit, minmax(260px, 1fr))`, gap 27.6px,
   padding `9.2 27.6 27.6`. Three sections in DOM order: **Fighting (i)**, **Being (ii)**,
   **Standing (iii)**.
   - Section head: flex row, padding-bottom 4.6px, **1px bottom border `#b68235`**. Title is `h6`
     (Cormorant Garamond 600, 13px, uppercase, letter-spacing .08em) in `#7d5411`; a lowercase
     roman numeral sits flush right in Courier Prime 11px `#7d7979`. Numerals must stay in reading
     order i → iv.
   - Stat row: 2-column grid `1fr auto`, baseline-aligned, gap 13.8px, padding 7px 0,
     1px bottom divider (`rgba(32,31,29,.16)`) except on the last row of each section.
     Label = Lora 13px, letter-spacing .03em, `#605d5d`. Value = Cormorant Garamond 20px with
     tabular figures, `#201f1d`; **zero/absent values drop to `#9b9797`**
     (`--color-neutral-500`) so they read as "none" rather than data.
   - Qualifier text inside a value (e.g. `normal` after Speed 110) is Courier Prime 11px `#7d7979`.
7. **History** — full width, padding `0 27.6 27.6`. Same section head pattern, numeral `iv`.
   Body panel: margin-top 13.8px, padding 18.4px, background `#f8f4f4`, 1px divider border,
   radius 4px; text Courier Prime 14px, line-height 1.75, `#444141`, `text-wrap: pretty`.
   The game's raw history string is re-wrapped by the panel — do **not** preserve the engine's
   hard line breaks or double spaces.
8. **Keybinding strip** — flex row, wraps, gap 13.8px, padding 13.8px / 27.6px, 1px top divider,
   background `#f8f4f4`, Courier Prime 12px `#605d5d`. Bracketed keys are `#7d5411`.
   Items: `[esc] return to dungeon`, `[c] change name`, `[f] write character dump`, then a
   right-aligned status `turn 0 · no save since start`.

### Exact content used
| Section | Rows (label → value) |
| --- | --- |
| Header | Rookie / Steven / Human Warrior of the first depth / Level 1 / Gold 134 |
| Meters | Hit points 20 / 20 · Mana 0 / 0 · Next level in 0 / 10 |
| Fighting (i) | Armour class 10 · To hit +4 · To damage +3 · Blows / round 2.77 · Shots / round 0.0 |
| Being (ii) | Speed 110 *normal* · Infravision 0 · Light radius 0 · Age 18 · Height / weight 56 / 159 · Burden 191 |
| Standing (iii) | Experience 0 · Current depth Town · Deepest reached — · Title Rookie |
| Standing note | *No dungeon level has yet been entered. The gate stands open to the north of town.* (Lora italic 13px, line-height 1.6, `#605d5d`, margin-top 18.4px) |
| History (iv) | You are one of several children of a Yeoman. You are a credit to the family. You have hazel eyes, straight brown hair, and an average complexion. |

### Copy decisions to confirm
- `Depth 0` renders as **Town**; `Deepest 0` renders as an em dash. Substitute literal `0` if the
  team prefers raw engine values.
- Name / Race / Class / Title moved into the header; Title is repeated once in **Standing** as the
  progression-facing field. The original's separate "Character" frame is therefore gone.
- `To hit` / `To damage` gain an explicit `+` sign.
- The subhead ("…of the first depth") and the Standing note are derived flavour text; drive them
  from depth or drop them if the engine has no equivalent string.

## Interactions & Behavior
The window is read-only; there are no controls in the design.
- Keys in the footer are **hints**, not buttons: `Esc` closes the window, `c` opens the existing
  change-name flow, `f` writes the character dump. Wire to the client's existing bindings.
- No hover states are required (nothing is interactive). If the port makes the footer hints
  clickable, use the design system's states: hover tint `rgba(182,130,53,.12)`, pressed
  `rgba(182,130,53,.22)`, focus ring `2px solid #b68235` at 2px offset — never a platform default.
- No animations or transitions. Meter fills are static; if the window is live-updated on turn
  boundaries, snap the width (no easing) so it matches the game's tick.
- Responsive: both grids are `auto-fit` with min track widths (220px meters, 260px stats), so the
  sheet collapses 3 → 2 → 1 column as the window narrows. The identity header wraps the display
  figures below the name block. Nothing has a fixed height; the sheet grows with the history text.

## State Management
No UI state. The view is a pure render of the character record the client already holds:
`name, race, class, title, level, exp, exp_to_next, gold, depth, max_depth, hp, hp_max, sp, sp_max,
ac, to_hit, to_dam, blows, shots, speed, infravision, light, age, height, weight, burden, history[]`.
Derived at render time: the three meter ratios (`hp/hp_max`, `sp/sp_max`, `exp/exp_to_next`, each
guarded for a 0 maximum), the depth labels, and the muted-colour test (`value == 0`).
Refresh on the same event the current Tk window refreshes on.

## Design Tokens
From `classical-tokens.css` (bundled). Use the variables, not the literals, wherever the target
supports them.

**Colour** — bg `#f3f2f2` · surface `#eae9e9` · text `#201f1d` · accent `#b68235` ·
divider `rgba(32,31,29,.16)`
Neutral ramp: 100 `#f8f4f4` · 200 `#eae7e7` · 300 `#d7d3d3` · 400 `#bab6b6` · 500 `#9b9797` ·
600 `#7d7979` · 700 `#605d5d` · 800 `#444141` · 900 `#2d2b2b`
Accent ramp: 100 `#fff3e4` · 200 `#ffe3bf` · 300 `#facb8d` · 400 `#e1ad66` · 500 `#c28d41` ·
600 `#a06f24` · 700 `#7d5411` · 800 `#5a3b0a` · 900 `#3a270d`
Accent body text always uses **700** (`#7d5411`); bare accent is for strokes and large type only.

**Spacing** 4.6 / 9.2 / 13.8 / 18.4 / 27.6 / 36.8 px (1.15× density scale).

**Radius** sm 2 · md 4 · lg 7 px.

**Shadow** sm `0 1px 2px rgba(45,43,43,.14)` · md `0 3px 10px rgba(45,43,43,.16)` ·
lg `0 12px 32px rgba(45,43,43,.22)`. Only `lg` is used (on the sheet card).

**Type**
- Headings: **Cormorant Garamond** — 46px/400 (name), 62px/300 (display figures), 20px/600
  (stat values), 13px/600 uppercase .08em (`h6` section heads), 14px (title-strip label).
  Rule: the larger the type, the lighter the weight; never bold.
- Body: **Lora** — 17px italic (subhead), 13px (stat labels, flavour note).
- Mono: **Courier Prime** — 46px (`@`), 14px (history), 10–12px (kickers, numerals, footer).
  This is the one addition to the design system, used deliberately to carry the roguelike/terminal
  register; keep it to the sigil, history, kickers, numerals and footer.
- All figures set with tabular numerals (`font-feature-settings: 'tnum'`). Running prose keeps
  proportional figures.

## Tk implementation notes
- Fonts: `font create` three named fonts (`{Cormorant Garamond}`, `Lora`, `{Courier Prime}`) with
  a documented fallback chain (`Times`/`Georgia`, `Courier`) — Courier at minimum must resolve.
- Letter-spacing and `tnum` have no Tk equivalent. For the small uppercase kickers, insert a thin
  space between characters or accept the tighter setting; for figure alignment rely on Courier's
  natural monospacing in tabular contexts.
- Hairlines: 1px `frame` widgets with `-bg` set to the divider/accent colour and
  `-height 1`, or `canvas create line`; do not use `relief`/`groove` borders — they read as 3D.
- The sheet card's rounded corners and shadow are not reproducible in plain Tk. Acceptable
  simplification: square card, 1px `#bab6b6` border, no shadow, on the `#eae7e7` ground.
- Meters: a `frame` track (`-bg #f3f2f2`, `-highlightthickness 1`,
  `-highlightbackground` accent/neutral-400) containing a fill frame placed with `place -relwidth`.
- Text panels: `text` widget with `-relief flat -borderwidth 0 -highlightthickness 0 -wrap word`,
  `-padx/-pady 18`, `-bg #f8f4f4`, and `-state disabled` after insert.

## Assets
None. No images, icons or fonts are shipped. The `@` sigil is a text glyph, not an asset. The three
Google Fonts (Cormorant Garamond, Lora, Courier Prime) are all open-licence — vendor the TTFs with
the client rather than loading them at runtime. `before-tk-original.png` in this folder is the
current Tk window, included only for comparison.

## Files
- `Character Sheet.dc.html` — the design prototype. Open in a browser; it is a single self-contained
  HTML file whose only external references are `classical-tokens.css` (see next item) and the Google
  Fonts stylesheet. To view it standalone, point its `<link rel="stylesheet">` at
  `classical-tokens.css` in this folder.
- `classical-tokens.css` — the Classical design system stylesheet: token `:root` block plus the
  component layer. The authoritative source for every colour, font and spacing value above.
- `before-tk-original.png` — screenshot of the existing Tk window being replaced.
