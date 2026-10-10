# Handoff: Zangband Knowledge window (classic Tk / X11 style)

## Overview
This replaces the current Knowledge window (`before-current-knowledge.png`), which looks like a web page: serif type, underlined text tabs, gold hairlines. The new version looks like a **1990s Tk application on X11**, built only from standard Tk widgets: Motif-era relief, the grey `#d9d9d9` palette, bold Helvetica labels, a Courier listbox and text, and diamond radio indicators filled `#b03060`.

The purpose and data are unchanged. The player picks a category, finds an entry and reads its recall.

## About the design files
`Knowledge Tk.dc.html` is an **HTML prototype**, not production code. It shows the intended look and behaviour so you can **rebuild it in Tcl/Tk with native widgets**. Every element below maps to a stock Tk widget. Most of the look comes from Tk's own defaults on X11, so in most places you set no colour options at all.

Open the prototype in a browser with `support.js` next to it, which is included in this folder.

## Fidelity
**High fidelity** for layout, widget choice, relief, colours and behaviour. Pixel values come from the web mock-up; let Tk's geometry managers size things. The monster list matches the screenshot. The recall text is placeholder (see "Data").

---

## Widget tree (Tk)

```
.know                     toplevel   -title "Zangband: Knowledge"
├─ menu .know.mb          menubar (File / View / Help), -tearoff 1 on each submenu
├─ .know.cat              frame      category row
│   ├─ label  "Category:"
│   └─ radiobutton ×7     -variable ::know(cat)  (Monsters … Traps)
├─ .know.tools            frame
│   ├─ label "Find:" -underline 0 + entry .find   (-textvariable ::know(query))
│   └─ label "Sort by:" + tk_optionMenu .sort ::know(sort) Depth Name "Times seen"
├─ .know.body             frame (pack/grid two columns)
│   ├─ labelframe .list   -text "Monsters — 32 known"
│   │   ├─ frame hdr: label "Name" (w)  label "Depth" (e)   -font normal weight
│   │   ├─ listbox .lb    -font {Courier 12} -bg white -selectmode browse
│   │   │                 -exportselection 0 -activestyle none
│   │   └─ scrollbar .sb  -command {.lb yview};  .lb -yscrollcommand {.sb set}
│   └─ frame .detail
│       ├─ canvas .glyph  40×40 -bg black -relief sunken -bd 2  (one text item)
│       ├─ label .name    -font {Helvetica 18 bold}
│       ├─ label .kind    -font {Helvetica 12 italic}
│       ├─ labelframe .facts -text "Facts"   (grid of label + sunken label)
│       └─ labelframe .known -text "What Is Known"
│           ├─ text .txt  -font {Courier 12} -wrap word -bg white -state disabled
│           └─ scrollbar  linked to .txt
├─ frame -height 2 -bd 1 -relief sunken          (separator)
├─ .know.btns             frame (right-aligned)
│   ├─ button "Previous"  -underline 0
│   ├─ button "Next"      -underline 0
│   └─ button "Close"     -underline 0 -default active
└─ .know.status           frame
    ├─ label .msg   -relief sunken -bd 1 -anchor w   (expands)
    └─ label .pos   -relief sunken -bd 1             ("5 of 32")
```

The window-manager frame in the mock-up (title bar, iconify/maximize, desktop icon) is just there to set the scene. In the real app the WM draws it. Don't build it.

---

## Layout
- **Menubar**: Tk menubar, raised 2px. Mnemonics are underlined: **F**ile, **V**iew, **H**elp.
- **Category row**: `padx 10, pady {10 4}`. Radiobuttons are packed left with a ~16px gap and wrap if the window is narrow (use `grid` with a reflow, or one row is fine).
- **Tools row**: the Find entry expands (`-fill x -expand 1`); Sort sits on the right.
- **Body**: two columns.
  - Left labelframe: about 1/3 of the width, minimum about 300px. The listbox shows **22 rows** (`-height 22`).
  - Right column: about 2/3, minimum about 440px. Header (glyph, name, kind), then Facts, then What Is Known, which takes the remaining height (`-fill both -expand 1`).
  - Gap between columns about 12px. Below about 760px, stack the columns.
- **Facts grid**: label/value pairs, **two pairs per row** (columns: label, value, label, value). Labels are right-aligned in normal weight with a trailing colon. Values are `label -relief sunken -bd 1 -anchor w` in bold. Gaps about 6px vertical, 18px between pairs.
- **What Is Known**: text widget, minimum about 8 lines, with its own scrollbar.
- **Buttons**: right-aligned, `padx 10 pady 10`, about 10px apart, all the same width (`-width 10`).
- **Status bar**: two sunken labels. The message label fills; the position label sits on the right.

## Colours (Tk X11 defaults; set explicitly only if your platform differs)
| Role | Value | Tk option |
|---|---|---|
| Background | `#d9d9d9` | `-background` |
| Active / hover | `#ececec` | `-activebackground` |
| Light bevel | `#ffffff` | (computed by Tk from bg) |
| Dark bevel | `#828282` | (computed by Tk from bg) |
| Trough, listbox selection | `#c3c3c3` | `-troughcolor`, `-selectbackground` |
| Radio/check indicator fill | `#b03060` | `-selectcolor` |
| Disabled text | `#a3a3a3` | `-disabledforeground` |
| Entry / listbox / text field | `#ffffff` | `-background` on those widgets |
| Focus ring | `#000000`, 1px | `-highlightthickness 1 -highlightcolor black` |

**Listbox selection**: background `#c3c3c3`, black text, `-selectborderwidth 1`. The selected row looks slightly raised, as in Tk on X11. Don't use a blue highlight.

**Glyph colours** (canvas text on black): use the game's own colour table, the same as the main map. The mock-up uses approximations such as townsperson umber `#c88a44`, the green wretch `#2fa82f`, rogue blue `#4a6aff` and agent violet `#d040d0`.

## Typography
- UI labels, buttons, menus: `{Helvetica 12 bold}` (Tk's classic X11 default).
- Secondary labels (column heads, fact names, status): `{Helvetica 12}`.
- Entry name: `{Helvetica 18 bold}`. Kind: `{Helvetica 12 italic}`.
- Listbox, text and the Find entry: `{Courier 12}`. Glyph: `{Courier 20 bold}`.
- In the text widget, the opening phrase ("The boil-covered wretch ('t')") uses a `bold` tag: `.txt tag configure head -font {Courier 12 bold}`.

---

## Interactions & behaviour
| Action | Result |
|---|---|
| Click a category radiobutton or View ▸ category | Swap the list to that category, scroll to top, restore that category's last selection (default first row). Status: "Objects: 8 known." |
| Type in Find | Live case-insensitive substring filter on the name. Keep the selection if it still matches, otherwise select the first row. Empty result shows "No matches."; an empty category shows "Nothing is known yet." |
| Sort by (option menu) | Depth (native depth, then game order), Name (A→Z), Times seen (descending). |
| Click a list row | Select it, focus the listbox, fill in the detail pane, scroll the text back to the top. Status: "Recalled: {name}." |
| ↑ / ↓ / Home / End / PgUp / PgDn in the listbox | Move the selection and keep it in view (`.lb see $i`). |
| Previous / Next | Move the selection by one. Disable at the ends (`-state disabled`). |
| Close (default button), Esc, File ▸ Close | Close the window (`wm withdraw` or `destroy`). Esc closes an open menu first. Return triggers Close. |
| File ▸ Dump to File… (Ctrl+S) | Write the knowledge text to a file and report the result in the status bar. |
| Help ▸ On Knowledge… (F1) | Short usage hint in the status bar, or a `tk_messageBox`. |
| Scrollbars | Standard Tk: arrows step one row (one line for text), trough click pages, slider drags. |

**Bindings**
```tcl
bind .know <Escape>     {destroy .know}
bind .know <Return>     {.know.btns.close invoke}
bind .know <Alt-f>      {focus .know.tools.find}
bind .know <Alt-p>      {.know.btns.prev invoke}
bind .know <Alt-n>      {.know.btns.next invoke}
bind .know <Control-s>  {knowDump}
bind .lb <<ListboxSelect>> {knowShow [.lb curselection]}
```

## State
```tcl
array set ::know {cat monsters  sort depth  query ""}
# ::know(sel,<cat>) = name last selected in each category (persists while the window is open)
```
- The list is rebuilt from `filter(query) ∘ sort(sort)` over the category's records.
- The detail pane is a pure function of the selected record.
- The position label shows "{index} of {filtered count}".

## Data (per record)
| Field | Use |
|---|---|
| `name` | Listbox, title (first letter capitalised) |
| `right` | Listbox right column: depth (`town` or a number) for monsters and traps, level for objects, symbol for terrain |
| `glyph`, `color` | Canvas glyph |
| `kind` | Italic line under the name (e.g. "townsfolk") |
| `facts` | Ordered `{label value}` pairs |
| `recall` | Head phrase (bold) + body, from the game's existing recall generator |

**Monster facts, in order:** Native depth ("Town" or "Level N (N×50 ft)"), Speed ("Normal (+0)" or "+N"), Hit points, Armour class, Experience, Times seen, Slain, all lives.
**Objects:** Level, Weight, Base cost, Times seen. **Terrain:** Symbol, Passable, Blocks sight. **Traps:** Native depth, Times seen, Disarmed.

The 32 monsters match the current screen. Their descriptions and all object, terrain and trap entries are **placeholders**; use the game's real `lore` / `k_info` / `f_info` text.

## Prototype-only extras (do not implement)
- The WM title bar, iconify-to-desktop icon and maximize. Your window manager provides these.
- The **Tweaks**: diamond vs. circle indicators (Tk 4-era diamonds vs. later circles; your Tk version decides) and frame colour.
- The checkered desktop background.

## Files
- `Knowledge Tk.dc.html`: the interactive prototype (needs `support.js` beside it).
- `support.js`: the prototype runtime (needed only to view the prototype).
- `before-current-knowledge.png`: the current screen being replaced.
