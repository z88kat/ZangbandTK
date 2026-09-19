# Handoff: Zangband Items window (paper doll + inventory)

## Overview
Redesign of the "Items" window in a Zangband (Angband-family roguelike) client currently built in
Tk/Tcl. The original is a blue canvas with a grey mannequin bitmap, twelve coloured square slots
joined to it by white leader lines, and a flat icon grid for the inventory.

The redesign keeps the same two-pane model — **Equipment** (paper doll, left) and **Inventory**
(right) — and the same drag-and-drop interaction, but restyles both to match the already-delivered
Character Sheet: parchment ground, hairline rules, Cormorant/Lora/Courier type, gold as stroke only.
The mannequin becomes a pen-and-ink adventurer in the early-D&D woodcut idiom; item icons become
roguelike ASCII glyphs.

Companion document: `design_handoff_character_sheet/README.md`. Both windows share one visual
system — the title strip, section-head pattern, divider rules, numerals and footer key strip are
identical by design and should be implemented once and reused.

## About the Design Files
The files here are **design references created in HTML** — a working prototype of the intended look
and behaviour, not production code to port line-for-line. Recreate the design in the target
environment (Tk/Tcl widgets in the existing client, or whatever toolkit it moves to). Where a widget
cannot reproduce something, approximate it — see "Tk implementation notes".

## Fidelity
**High-fidelity.** Colours, type, spacing and rules are final, taken from the bound "Classical"
design system (`classical-tokens.css`, bundled). Item names, weights and the starting loadout are
**placeholder sample data** — the real values come from the engine.

## Screens / Views

### Items window (single view, two panes)
**Purpose**: the player inspects what is worn and carried, and moves items between the two by
dragging; then returns to the dungeon.

**Shell** — identical to the Character Sheet: desk ground `#eae7e7` with 27.6px padding; sheet card
`max-width: 1180px`, background `#f3f2f2`, 1px border `#bab6b6`, radius 7px, shadow
`0 12px 32px rgba(45,43,43,.22)`, `overflow: hidden`. Spacing scale: `--space-1..8` =
4.6 / 9.2 / 13.8 / 18.4 / 23 / 27.6 / 32.2 / 36.8 px.

1. **Title strip** — flex row, padding 9.2 / 18.4, background `#f8f4f4`, 1px bottom divider.
   `ZANGBAND` in Courier Prime 12px, .22em, uppercase, `#7d7979`; flexible 1px hairline; `ITEMS`
   in Cormorant Garamond 14px, .16em, uppercase, `#7d5411`.
2. **Inscription bar** — flex row, wraps, gap 13.8px, padding 13.8 / 27.6, 1px bottom divider.
   Label `INSCRIPTION` (Courier Prime 11px, .2em, uppercase, `#7d7979`); a text input
   (`flex: 1 1 240px`, padding 6/10, Courier Prime 13px, 1px `#bab6b6` border, radius 2px,
   placeholder "none"); **focus**: border `#b68235` + `0 0 0 2px #fff3e4`, never the platform ring.
   Trailing italic Lora 13px `#7d7979` help line: "Drag an item onto a slot to wield or wear it;
   drag it off to stow it."
3. **Two panes** — CSS grid `repeat(auto-fit, minmax(520px, 1fr))`, no gap; the left pane carries a
   1px right divider. Below ~1080px the panes stack to one column.
4. **Footer key strip** — flex row, wraps, gap 13.8px, padding 13.8 / 27.6, 1px top divider,
   background `#f8f4f4`, Courier Prime 12px `#605d5d`, bracketed keys `#7d5411`:
   `[esc] close`, `[w] wear or wield`, `[t] take off`, `[d] drop`, then right-aligned
   `burden 14.8 / 90.0 / 150.0 lb` (tabular figures; the three numbers are current / limit /
   capacity, as in the original's `33.5/90.0/150.0 lb`).

#### Equipment pane (left, numeral **i**)
Padding 27.6px. Section head: flex row, padding-bottom 4.6px, 1px bottom border `#b68235`;
`h6` "Equipment" (Cormorant Garamond 600, 13px, uppercase, .08em) in `#7d5411`; lowercase roman
numeral flush right, Courier Prime 11px `#7d7979`.

Body is a 3-column grid — `minmax(148px,1fr) auto minmax(148px,1fr)`, `align-items: center`,
gap 13.8px, margin-top 23px — holding **six slot rows, the figure, six slot rows**.

- **Slot row** (a flex row; the left column uses `flex-direction: row-reverse` so it mirrors):
  `[hairline connector] [46×46 glyph box] [label + item name]`, gap 9.2px.
  - Connector: `flex: 1; min-width: 10px; height: 1px; background: var(--color-divider)`. This is
    the leader line that ties the slot to the figure — it must stay visible at every width.
  - Glyph box: 46×46, `flex: none`, 1px border `#bab6b6`, radius 2px, background `#f3f2f2`,
    centred glyph in Courier Prime 22px `#7d5411`.
  - Text block: `flex: 1 1 118px; min-width: 88px` (right-aligned in the left column). Slot label
    Courier Prime 9px, .18em, uppercase, `white-space: nowrap`, `#7d7979`; item name Lora 13px,
    line-height 1.3, `#201f1d`, `text-wrap: pretty`; an empty slot shows an em dash.
- **Slot order** — left column top to bottom: Head, Neck, Wielding, Shooting, Right hand, Feet.
  Right column: Light, Body, Cloak, Shield arm, Left hand, Hands. (Twelve slots, matching the
  engine's equipment array.)
- **Worn weight rule** — below the grid, margin-top 23px, padding-top 9.2px, 1px top divider:
  Courier Prime 12px `#605d5d`, "Worn weight" · flexible hairline · value in `#201f1d` tabular.

#### The figure
An inline SVG, `viewBox="0 0 140 330"`, rendered 118×278, in the column between the two slot
stacks. Style rules that define it:

- Two line weights only: **contour** `stroke: #2d2b2b; stroke-width: 1.6` and **interior/hatch**
  `stroke: #7d7979; stroke-width: 1`. Round caps and joins. No thick outlines, no gradients.
- Two flat fills: cloak `#eae7e7`, figure/garment `#f8f4f4`. Nothing else is filled.
- Subject: a front-facing hooded adventurer — hood and drawn face, mail tunic with belt and square
  buckle, cloak sweeping behind both shoulders, arms tucked with gauntleted hands, greaved legs into
  boots. Diagonal hatching runs down both cloak edges (six strokes a side) — that hatching is what
  gives the 1970s pen-and-ink register; do not drop it.
- It is decorative and static: no drop targets sit on the figure itself (unlike the original, where
  the squares were pinned to the body). All targeting happens on the slot rows.
- Purely ornamental for assistive tech beyond its label — `aria-label="Pen-and-ink figure of the
  adventurer"`.

#### Inventory pane (right, numeral **ii**)
Padding 27.6px; same section-head pattern, numeral **ii**.

- **Item row** — grid `auto auto 1fr auto`, baseline-aligned, gap 13.8px, padding 9px 6px, 1px
  bottom divider, `cursor: grab`; **hover** background `#fff3e4`.
  Columns: selection key (Courier Prime 12px `#7d7979`, `a)`, `b)`, … assigned by position);
  ASCII glyph (Courier Prime 18px `#7d5411`); name (Lora 14px, line-height 1.35, `#201f1d`,
  `text-wrap: pretty`); weight (Cormorant Garamond 15px, tabular, `#7d7979`, one decimal).
- **Empty state** — "Your pack is empty." in Lora italic 13px `#7d7979`.
- **Message line** — below the list, Lora italic 13px, line-height 1.6, `#605d5d`. This is the
  window's equivalent of the game's message line; it reports the result of each drag (see below).
- **Carried weight rule** — same pattern as the worn-weight rule: item count ("5 items", singular
  "1 item") · hairline · total weight.

### Glyph vocabulary
Item icons are the game's own ASCII symbols set in Courier Prime, not bitmaps — this is deliberate:
it carries the roguelike register and removes the icon-asset dependency entirely.

| Glyph | Kind | | Glyph | Kind |
| --- | --- | --- | --- | --- |
| `\|` | weapon | | `!` | potion / flask |
| `}` | launcher (bow) | | `?` | scroll |
| `=` | ring | | `-` | wand |
| `"` | amulet | | `,` | food |
| `~` | light source | | `(` | soft armour, cloak |
| `]` | helm, gloves, boots | | `)` | shield |

An **empty slot shows its type's glyph** as an affordance (an empty ring slot shows `=`), so the
doll reads complete even with nothing equipped. Use the engine's real symbol table rather than this
excerpt; extend the map as item kinds require.

### Sample data in the prototype
Worn: Main Gauche `|` 3.0 · Soft Leather Armour `(` 8.0 · Wooden Torch `~` 3.0; nine slots empty.
Carried: 5 Rations of Food `,` 2.5 · 3 Flasks of oil `!` 3.0 · 2 Potions of Cure Light Wounds `!`
0.8 · a Scroll of Word of Recall `?` 0.5 · a Wand of Magic Missile `-` 0.3.
All placeholder — replace with the engine's inventory, in the engine's own sort order (the message
line notes this: wands before scrolls, scrolls before potions).

## Interactions & Behavior

### Drag and drop (the core interaction)
Every slot row and every inventory row is draggable. Both directions are supported, plus swap:

1. **Pack → slot (wear/wield)**: drop an item on a compatible slot; it leaves the pack and is worn.
   If the slot was occupied, the displaced item is **appended to the pack** in the same gesture.
2. **Slot → pack (take off)**: drop anywhere in the inventory pane; the item is unequipped and
   appended to the pack.
3. **Slot → slot**: allowed when the types match — the two ring slots are the real case.
4. **Rejected drop**: an incompatible pairing leaves state untouched and writes a refusal to the
   message line ("You cannot wear … there.").

**Type matching**: each slot declares an accepted type (`weapon`, `bow`, `ring`, `amulet`, `light`,
`body`, `cloak`, `shield`, `head`, `gloves`, `boots`); both ring slots accept `ring`. The prototype
matches on a single type string — the real client should defer to the engine's own
"can this go here" predicate, which knows about two-handed weapons, cursed items and class
restrictions the prototype does not model.

**Drag feedback** (applied to the hovered target, cleared on leave and on drop):
- Compatible: background `#fff3e4`, `outline: 1px solid #b68235`, `outline-offset: 3px`.
- Incompatible: background `rgba(160,111,36,.07)`, `outline: 1px dashed #bab6b6`, offset 3px.
  The target still lights — the player sees the slot is *understood* but refused, which is why the
  invalid state is drawn rather than ignored.
- Dragging an empty slot is prevented outright (no drag starts).

**Message line copy**: success wearing → "You are using {item}."; success removing → "You were
using {item}."; refusal → "You cannot wear {item} there." Substitute the engine's real strings if
they exist — these mirror Angband's phrasing.

### Other behaviour
- Weight totals (worn, carried, burden) recompute on every change; one decimal place, tabular
  figures. The burden limit numbers (90.0 / 150.0) come from the character's strength.
- The inscription field is a plain text input; it holds the inscription to apply and has no other
  behaviour in the prototype. Wire it to the client's existing inscription command.
- Footer keys are **hints, not buttons** — `Esc` closes, `w` wear/wield, `t` take off, `d` drop.
  Keyboard parity is required: drag-and-drop is an addition to the key commands, never a
  replacement, and the letter keys (`a)`, `b)`, …) remain the primary selection method.
- If the footer hints are made clickable in the port, use the system's states: hover
  `rgba(182,130,53,.12)`, pressed `rgba(182,130,53,.22)`, focus `2px solid #b68235` at 2px offset.
- No animation. State changes are instant, matching the game's turn model.
- **Accessibility gap to close in implementation**: pointer drag is currently the only way to
  move an item with the mouse. Keep the key commands wired, and expose slots/rows as focusable
  controls with an activate-to-move fallback.

### Responsive behaviour
- Panes: `auto-fit` at a 520px min track — two columns above ~1080px, stacked below.
- The equipment grid's side columns have a 148px floor; the text block inside has an 88px floor and
  slot labels never wrap (`white-space: nowrap`). These floors are what stop the doll degrading into
  stacked fragments at narrow widths — preserve them.
- Nothing has a fixed height; both panes grow with their content.

## State Management
No persisted UI state beyond the inscription text. The view is a render of two engine-owned
collections plus one transient value:

- `slots` — a map of the twelve slot ids (`weapon, bow, ringRight, ringLeft, neck, light, body,
  cloak, shield, head, gloves, boots`) to an item or null.
- `bag` — an ordered array of items; position determines the `a)`–`z)` selection key.
- Item shape: `{ glyph, name, weight, type }`.
- `inscription` — the text field's value.
- `message` — the last result string shown in the message line.
- Transient drag descriptor, held outside render state: `{ from: "bag"|"slot", index|slot }`.

Derived at render: selection keys, the three weight totals, the item count label, empty-slot glyphs
and the empty-pack flag. In the real client, `slots` and `bag` are **views onto the engine's
inventory** — every drop should call the engine's equip/unequip command and re-render from the
result, not mutate a local copy, so the game stays authoritative (turn cost, curse checks, messages).

## Design Tokens
From `classical-tokens.css` (bundled) — use the variables, not the literals, wherever the target
supports them. Identical to the Character Sheet handoff.

**Colour** — bg `#f3f2f2` · surface `#eae9e9` · text `#201f1d` · accent `#b68235` ·
divider `rgba(32,31,29,.16)`
Neutral: 100 `#f8f4f4` · 200 `#eae7e7` · 300 `#d7d3d3` · 400 `#bab6b6` · 500 `#9b9797` ·
600 `#7d7979` · 700 `#605d5d` · 800 `#444141` · 900 `#2d2b2b`
Accent: 100 `#fff3e4` · 200 `#ffe3bf` · 300 `#facb8d` · 400 `#e1ad66` · 500 `#c28d41` ·
600 `#a06f24` · 700 `#7d5411` · 800 `#5a3b0a` · 900 `#3a270d`
Accent text always uses **700**; bare accent is for strokes and large type only.

**Spacing** 4.6 / 9.2 / 13.8 / 18.4 / 23 / 27.6 / 32.2 / 36.8 px · **Radius** sm 2 · md 4 · lg 7 px
**Shadow** lg `0 12px 32px rgba(45,43,43,.22)` (the card only; no other elevation is used).

**Type**
- Cormorant Garamond — 15px (weights), 13px/600 uppercase .08em (`h6` heads), 14px (title strip).
- Lora — 13px (slot names, message line, help text), 14px (item names); italics carry flavour.
- Courier Prime — 22px (slot glyphs), 18px (item glyphs), 9–13px (labels, keys, footer, input).
- Figures tabular (`font-feature-settings: 'tnum'`); running prose keeps text figures.

## Tk implementation notes
- Reuse the Character Sheet's named fonts and fallback chain; Courier must resolve at minimum.
- The figure: keep it as vector line art. A Tk `canvas` reproduces it directly —
  `create_line`/`create_polygon` with `-width 1.6`/`-fill #2d2b2b` for contours and
  `-width 1`/`-fill #7d7979` for hatching, over `-fill #eae7e7`/`#f8f4f4` polygons. If the port
  keeps a bitmap instead, export the SVG at 2× and 3× for hi-dpi. Do **not** re-use the old grey
  mannequin.
- Slot rows: `frame` per row with a `canvas`/`label` glyph box; hairline connectors are 1px frames
  with `-bg` = divider colour. Avoid `relief`/`groove` borders — they read as 3D.
- Drag and drop: Tk has no native DnD. Bind `<ButtonPress-1>`, `<B1-Motion>`, `<ButtonRelease-1>`;
  carry a toplevel `-overrideredirect` ghost showing the item glyph, and hit-test slots on release
  via `winfo containing`. Recolour the hovered slot's border/background per the feedback spec above.
- Hover/focus states must be applied explicitly (`<Enter>`/`<Leave>` bindings) — Tk gives none free.
- Rounded corners and the card shadow are not reproducible in plain Tk. Acceptable simplification:
  square card, 1px `#bab6b6` border, no shadow, on the `#eae7e7` ground.

## Assets
None to ship. No bitmaps, no icon sheet — item icons are text glyphs and the figure is inline
vector art (its path data is in `Items.dc.html`, the `<svg>` in the equipment pane). The three
Google Fonts (Cormorant Garamond, Lora, Courier Prime) are open-licence; vendor the TTFs with the
client rather than fetching at runtime.

## Files
- `Items.dc.html` — the design prototype, with working drag and drop. Open in a browser. Its only
  external references are `classical-tokens.css` and the Google Fonts stylesheet; to view it
  standalone, point its `<link rel="stylesheet">` at `classical-tokens.css` in this folder.
- `classical-tokens.css` — the Classical design system stylesheet; authoritative for every value
  above.
- `before-tk-original.png` — the existing Tk Items window, for comparison.
