# Handoff: Zangband Character window (classic Tk / X11 style)

## Overview
This replaces the current Character window (`before-current-character.png`), which is in the serif, web-page style. The new version is a **1990s Tk-on-X11 application** that matches the Knowledge window handoff (`design_handoff_knowledge_tk/`). The colours, fonts, relief, menubar, radiobutton indicators, buttons and status bar are identical, so build them once and share them.

**Main structural change:** History used to be a cramped strip at the bottom. It is now its own **page** (Info / History / Virtues / Mutations / Notes), where the text has room.

## About the design files
`Character Tk.dc.html` is an **HTML prototype**, not production code. Rebuild it in Tcl/Tk with stock widgets. Open it in a browser with `support.js` alongside it. Most of the look comes from Tk's X11 defaults, so set few or no colour options.

## Fidelity
**High fidelity** for layout, widget choice, relief, colours and behaviour. Values match the current screen, with two exceptions marked ⚠ below.

---

## Widget tree (Tk)

```
.char                       toplevel  -title "Zangband: Character"
├─ menu .char.mb            menubar: File / View / Help (each -tearoff 1)
├─ .char.top                frame, two columns (wrap below ~800px)
│   ├─ labelframe .id  -text "Character"
│   │   ├─ canvas .glyph   64×64 -bg black -relief sunken -bd 2   text "@" white {Courier 32 bold}
│   │   └─ frame .fields   grid, 2 pairs per row: label "Name:"  label(sunken) …
│   └─ labelframe .cond -text "Condition"
│       └─ 3 rows × {label  canvas-bar  label(sunken)}
├─ .char.pages              frame: label "Page:" + radiobutton ×5  -variable ::char(page)
├─ .char.page               frame; contents swapped per page (pack forget / pack)
│   ├─ info:      3 × labelframe (Fighting, Being, Standing), grid of label + label(sunken)
│   ├─ history:   labelframe "Background" → text -wrap word -state disabled -bg white
│   ├─ virtues:   labelframe "Virtues" → grid of label + label(sunken) + note label
│   ├─ mutations: labelframe "Mutations" → listbox (or disabled text) "You have no mutations."
│   └─ notes:     labelframe "Notes" → text (editable) -bg white
├─ frame -height 2 -bd 1 -relief sunken          (separator)
├─ .char.btns   "Change Name…"  "Dump to File…"  <spacer>  "Close" (-default active)
└─ .char.status label(sunken, expands) "…"  +  label(sunken) "Turn 1"

.rename                     toplevel, transient .char, grab set    -title "Change Name"
├─ label "Enter a new name for your character:"
├─ entry -width 20 -font {Courier 12}   (max 15 characters)
├─ separator
└─ buttons: "OK" (-default active)  "Cancel"
```

The WM title bar, iconify-to-desktop icon and maximize in the mock-up only set the scene. The window manager provides them.

---

## Layout
- **Top row** has two labelframes side by side. Character takes about 55% and Condition about 45%. Below about 800px they stack.
  - **Character:** a 64px glyph canvas, then a grid of 4 rows × 2 pairs. Labels are right-aligned, normal weight, with a trailing colon (column about 52px). Values are bold sunken labels (`-relief sunken -bd 1 -anchor w`).
    Order: Name / Level, Race / Gold, Class / Depth, Title / Sex ⚠.
  - **Condition:** rows Hit points, Mana, Experience. Columns: right-aligned label (about 76px), bar (expands), and a sunken label with the value right-aligned (about 64px).
- **Page row:** "Page:" then five radiobuttons about 16px apart, with mnemonics underlined (**I**nfo, **H**istory, **V**irtues, **M**utations, **N**otes).
- **Page area:** `padx 10`, minimum height about 250px so switching pages doesn't make the window jump.
  - **Info:** three equal labelframes in a row (they wrap below about 780px). Each row is a label (left, normal weight, colon) plus a sunken value about 104px wide, **right-aligned**.
  - **History:** text widget, minimum 8 lines, line length capped at about 68 characters.
  - **Virtues:** two pairs per row. Underneath is the note "Your virtues are untested. Deeds in the dungeon will shift them."
  - **Notes:** editable text, 8 lines, resizable.
- **Buttons:** Change Name… and Dump to File… on the left, Close on the right, `padx/pady 10`.

## Condition bars (canvas)
Classic Tk has no progress bar, so draw one: a canvas about 14px high with `-bg #c3c3c3 -relief sunken -bd 1`, and a fill rectangle inset 2px whose width is `value / max`. Add a 1px light top-left and dark bottom-right edge on the fill so it looks raised.

| Bar | Fill | Value |
|---|---|---|
| Hit points | `#38a038` | `chp / mhp` |
| Mana | `#4060d0` | `csp / msp` (draw empty when max is 0) |
| Experience | `#c09020` | progress to next level |

Optional: shift the HP fill to yellow and then red at the game's usual warning thresholds.

## Colours and typography
Same as the Knowledge handoff:
- Background `#d9d9d9`, active `#ececec`, trough and listbox selection `#c3c3c3`, bevels `#ffffff` / `#828282`.
- Indicator fill `#b03060`, disabled text `#a3a3a3`, fields `#ffffff`, 1px black focus ring.
- Fonts: labels, buttons and menus `{Helvetica 12 bold}`; secondary labels `{Helvetica 12}`; text widgets and entries `{Courier 12}`; the `@` glyph white `{Courier 32 bold}`.

---

## Data (from the current screen)
| Group | Fields |
|---|---|
| Character | Name **Steven**, Level **1**, Race **Human**, Gold **140**, Class **Warrior**, Depth **Town**, Title **Rookie**, Sex ⚠ |
| Condition | HP **20 / 20**, Mana **0 / 0**, Experience **0 / 10** |
| Fighting | Armour class **10**, To hit **+4**, To damage **+3**, Blows / round **2.77**, Shots / round **0.0** |
| Being | Speed **110 (Normal)**, Infravision **0 ft**, Light radius **0**, Age **18**, Height / weight **55 / 228**, Burden ⚠ |
| Standing | Experience **0**, Current depth **Town**, Deepest reached **—**, Title **Rookie** |
| History | "You are one of several children of a Guildsman. You are a well liked child. You have brown eyes, straight brown hair, and a very dark complexion." |
| Virtues | The class's virtue set from the engine. The prototype shows eight, all "Neutral". |
| Mutations | The engine's mutation list. Empty state: "You have no mutations." |
| Notes | Free text, saved with the character. |

⚠ **To confirm:**
- **Sex** was not on the current screen; I added it. Drop it if you don't want it.
- **Burden**: the current screen shows "169". The prototype shows **"16.9 lb"**, assuming the game stores weight in tenths of a pound. Use the game's own weight formatting.

**Formatting rules:**
- Signed bonuses always carry their sign (`+4`, `-1`).
- Speed is shown as the raw number plus a word (`110 (Normal)`, `120 (+10)`).
- Deepest reached is an em dash until the player first goes down.

---

## Interactions & behaviour
| Action | Result |
|---|---|
| Page radiobutton, View ▸ page, or Alt+mnemonic | Swap the page frame. Remember the choice while the window is open. |
| Change Name… (File menu, or Alt+N) | Open the modal `.rename` dialog with the current name pre-selected. **Return** = OK, **Esc** = Cancel. A blank name means no change. On OK, update the Name field, the WM title if it shows the name, and the status line ("You are now known as …"). |
| Dump to File… (Ctrl+S) | Call the existing character-dump routine and report "Character dump written to {file}." |
| Close (default button), Esc, File ▸ Close | Close the window. Esc closes the dialog first, then any open menu, then the window. |
| Notes text | Editable. Save to the character on change or on close. |
| Help ▸ On Character… (F1) | Show a hint in the status bar. |

```tcl
bind .char <Escape>    {destroy .char}
bind .char <Return>    {.char.btns.close invoke}
bind .char <Control-s> {charDump}
bind .char <Alt-n>     {charRename}
foreach {k p} {i info h history v virtues m mutations} { bind .char <Alt-$k> [list set ::char(page) $p] }
```

## State
```tcl
array set ::char {page info}
# The window reads everything else from the live player record; refresh on the game's update hook.
```

## Prototype-only extras (do not implement)
- The WM frame, iconify-to-desktop and maximize.
- The **Tweaks**: diamond vs. circle indicator, and frame colour.
- The checkered desktop background.

## Files
- `Character Tk.dc.html`: the interactive prototype (needs `support.js` beside it).
- `support.js`: the prototype runtime (needed only to view the prototype).
- `before-current-character.png`: the current screen being replaced.
