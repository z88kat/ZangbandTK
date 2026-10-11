# Handoff: Zangband Items window: paper doll + inventory (classic Tk / X11)

## Overview
This replaces the current Items window (`before-current-items.png`), which is in the serif, web-page style with a line-art doll. The new version is a **1990s Tk-on-X11 application** that matches the Knowledge and Character handoffs (`design_handoff_knowledge_tk/`, `design_handoff_character_tk/`). The colours, fonts, relief, menubar, labelframes, buttons and status bar are identical, so share one implementation.

The **paper doll** is a pixel-art sprite on a black panel, with twelve equipment slots joined to the figure by dotted lines. This handoff ships the **female doll** as the default. The male sprite is included as an alternative (see "Doll sprite").

## About the design files
`Items Tk.dc.html` is an **HTML prototype**, not production code. Rebuild it in Tcl/Tk with stock widgets. Open it in a browser with `support.js` beside it. The doll sprites are also supplied as ready-made PNGs, so you don't need to port the sprite data.

## Fidelity
**High fidelity** for layout, widget choice, relief, colours, the sprite and behaviour. The data matches the current screen.

---

## Widget tree (Tk)

```
.items                       toplevel  -title "Zangband: Items"
├─ menu .items.mb            menubar: File / Items / Help (each -tearoff 1)
├─ .items.body               frame, two columns (stack below ~940px)
│   ├─ labelframe .eq  -text "Equipment"
│   │   ├─ canvas .doll      -bg black -relief sunken -bd 2  (whole doll panel, see below)
│   │   └─ frame: label "Selected:" + label(sunken, expands)
│   │             label "Worn:"     + label(sunken) "11.4 lb"
│   └─ labelframe .inv -text "Inventory"
│       ├─ frame hdr: label "Item" (w)  label "Weight" (e)      -font normal weight
│       ├─ text .list  -font {Courier 12} -bg white -cursor arrow -state disabled
│       │              -tabs {… right}   (used as a list, see below)
│       ├─ scrollbar  linked to .list
│       └─ frame: label "Pack:" + label(sunken) "4 items of 23" + label(sunken) "5.5 lb"
├─ .items.tools              frame
│   ├─ label "Inscription:" -underline 2 + entry + button "Inscribe"
│   └─ label "Burden:" + canvas bar + label(sunken) "16.9 / 120.0 lb"
├─ frame -height 2 -bd 1 -relief sunken                          (separator)
├─ .items.btns   "Wear/Wield" "Take Off" "Drop" "Inspect"  <spacer>  "Close" (-default active)
└─ .items.status label(sunken, expands) message  +  label(sunken) "Capacity 240.0 lb"
```

The WM title bar, iconify-to-desktop icon and maximize in the mock-up only set the scene. The window manager provides them.

---

## The doll panel (one canvas)
A single `canvas` with `-bg black -relief sunken -bd 2`, about 14px / 10px padding inside. Lay it out as three columns:

| Left column | Centre | Right column |
|---|---|---|
| 6 slot rows | sprite image 120×240 | 6 slot rows |

**Slot row, left side:** `[label 52px, right-aligned] [well 40×40] [dotted leader to the sprite]`.
**Slot row, right side:** mirrored: `[dotted leader] [well] [label 52px, left-aligned]`.
Rows are 40px tall with a 4px gap. **The labels must be a fixed 52px wide** so all twelve wells line up in two straight columns.

| Left (top → bottom) | Right (top → bottom) |
|---|---|
| Helm `]` | Light `~` |
| Amulet `"` | Body `(` |
| Weapon `\|` | Cloak `(` |
| Bow `}` | Shield `)` |
| Ring `=` (right hand) | Ring `=` (left hand) |
| Boots `]` | Gloves `]` |

Canvas items per slot:
- **Label:** `text`, `{Helvetica 12}` normal weight, fill `#c0c0c0`.
- **Well:** `rectangle` 40×40, `-fill black -outline #606060 -width 1`.
- **Glyph:** `text` centred in the well, `{Courier 18 bold}`, filled with the item's game colour. An empty slot shows its type symbol in `#3a3a3a`.
- **Leader:** `line` from the well to the sprite edge, `-fill #606060 -dash .`.
- **Selected:** a second rectangle 2px outside the well, `-outline #ffff00 -dash .`.
- **Tooltip / Selected field:** the slot label is short, so the full item name goes in the "Selected:" read-out (and a balloon help if you have one).

**Sprite:** `image create photo dollF -file doll-female-120x240.png`, then `.doll create image $cx $cy -image dollF`. It's a 24×48 pixel sprite drawn at 5×. To scale it in Tk instead, load `doll-female-24x48.png` and use `$img copy $src -zoom 5`.

## Doll sprite
- `doll-female-120x240.png` is the **default**. She has blond hair, a green dress with a belt and yellow buckle, a navy cloak, dark leggings and brown boots.
- `doll-female-24x48.png`: the same sprite at 1×, for `-zoom`.
- `doll-male-120x240.png`: the alternative. He has a brown hood, mail tunic, red cloak and boots.

Suggested: choose the sprite from the character's sex (the Character window has a Sex field), so neither sprite is hard-coded.

Female palette, for recolouring or redrawing: hair `#c8a040` / `#f0d070`, skin `#e0aa80` / `#b07850`, lips `#b03838`, cloak `#1a2a60` / `#3a4a90`, dress `#2a6a3a` / `#4a9a5a`, belt `#804000`, buckle `#ffff00`, leggings `#3a2a20` / `#5a4030`, boots `#3a2010` / `#604020`.

---

## Inventory list
Use a `text` widget as the list rather than a listbox, because each row needs a **coloured glyph on a black cell**:
```tcl
.list tag configure glyph  -background black -font {Courier 12 bold}
.list tag configure g_$col -foreground $col        ;# one tag per game colour
.list tag configure sel    -background #c3c3c3 -relief raised -borderwidth 1
.list configure -tabs {3c 3.6c [expr {$w-8}] right}
# row:  "a)\t" <glyph tagged glyph+g_col> "\t3 Rations of Food\t2.4 lb\n"
```
- Rows: key `a)`…`w)` (from position), glyph, name (with `{inscription}` if set), weight (one decimal, " lb").
- The selected row has the `sel` tag (looks slightly raised, as in Tk on X11; not a blue highlight).
- Empty pack: "Your pack is empty." in `#828282`.
- Track the selection yourself: bind `<1>` to work out the row from `@x,y`, and use arrow keys and Home/End.

## Burden bar (canvas)
A canvas about 14px high, `-bg #c3c3c3 -relief sunken -bd 1`. The fill rectangle is inset 2px, with width `total / capacity` (capacity 240 lb). It is green `#38a038` while under the limit and gold `#c09020` once over it. A 2px **black tick at the weight limit** (120 / 240, which is 50%) stands 3px proud above and below. The read-out shows `16.9 / 120.0 lb` (total / limit). The status bar shows capacity.

## Colours and typography
Same as the other Tk handoffs:
- Background `#d9d9d9`, active `#ececec`, trough and selection `#c3c3c3`, bevels `#ffffff` / `#828282`.
- Disabled text `#a3a3a3`, fields `#ffffff`, 1px black focus ring.
- Fonts: labels, buttons and menus `{Helvetica 12 bold}`; secondary `{Helvetica 12}`; list, entry and glyphs `{Courier 12}`.
- Glyph colours use the game's colour table. In the mock-up: dagger `#ffffff`, armour, torch and food `#c88a44`, potion `#ff8a1a`, scroll `#ffffff`.

---

## Interactions & behaviour
| Action | Result |
|---|---|
| Click a slot or inventory row | Select it (slots get the yellow dotted ring, rows the raised grey band). Update "Selected:". |
| Double-click an inventory row, Enter, `w`, Wear/Wield | Equip into the matching slot. Rings go to the free hand first. If the slot was occupied, the old item goes back to the pack. Status: "You are wielding/wearing …". |
| Double-click a slot, `t`, Take Off | Unequip to the pack (appended) and select it there. Status: "You were wearing …". |
| `d`, Drop | Drop the selected item (from slot or pack). |
| `i`, Inspect | Status: "You examine … It weighs … lb." (or open the game's item-info window). |
| Inscribe (or Enter in the entry) | Apply `{text}` to the selected item. Items ▸ Uninscribe clears it. |
| Buttons and menu items | Disabled (`#a3a3a3`) when they don't apply to the current selection. |
| Esc, Close (default), File ▸ Close | Close (Esc closes an open menu first). Letter keys are ignored while typing in the entry. |

**Drag and drop** (Tk has no native DnD; use press, motion and release):
- Inventory row → slot: equip (swap if occupied). Slot → inventory list: take off. Slot → slot: only between the two ring slots.
- During the drag, show a small `toplevel -overrideredirect 1` "ghost" with the glyph, and hit-test with `winfo containing` (or canvas `find overlapping`).
- **Feedback on the hovered well:** valid → 2px solid `#ffff00` ring; invalid → 2px solid `#ff0000` ring and the `X_cursor` cursor. Hovering the inventory list while dragging from a slot gives it a 2px black inner ring.
- An empty slot can't be dragged.
- Use the engine's own "can wear here" check (two-handed weapons, curses, class limits), not just the slot type.

```tcl
bind .items <Escape> {destroy .items}
bind .items <Return> {.items.btns.close invoke}
foreach {k cmd} {w itemsWear t itemsTakeOff d itemsDrop i itemsInspect} { bind .items <Key-$k> $cmd }
```

## State
```tcl
# ::items(sel) = {slot weapon} | {pack 2} | {}
# ::items(insc) = inscription entry text
# Everything else comes from the engine's equipment and inventory arrays; redraw after each command.
```
Every drop or button press should call the engine's equip / unequip / drop command and **redraw from the result**, so the game stays authoritative (turn cost, messages, curses).

## Data (from the current screen)
- **Worn:** Weapon: a Dagger (1d4) (+0,+0), 1.2 lb · Body: Soft Leather Armour [8,+0], 8.0 lb · Light: a Wooden Torch (5000 turns), 2.2 lb. The other slots are empty. Total **11.4 lb**.
- **Pack:** a) 3 Rations of Food 2.4 · b) a Potion of Berserk Strength 0.4 · c) a Scroll of Word of Recall 0.5 · d) a Wooden Torch (5000 turns) 2.2. Total **5.5 lb**.
- **Burden:** **16.9 / 120.0 lb**, capacity **240.0 lb**.

## Prototype-only extras (do not implement)
- The WM frame, iconify-to-desktop and maximize.
- The Tweaks: doll (female/male), frame colour, show/hide slot labels.
- The checkered desktop background.

## Files
- `Items Tk.dc.html`: the interactive prototype, female doll by default (needs `support.js` beside it).
- `support.js`: the prototype runtime (needed only to view the prototype).
- `doll-female-120x240.png`, `doll-female-24x48.png`, `doll-male-120x240.png`: the doll sprites for `image create photo`.
- `before-current-items.png`: the current screen being replaced.
