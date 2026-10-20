# Handoff: Zangband Character window, Flags page (classic Tk / X11)

## Overview
This replaces the Flags page (`before-flags-*.png`), the one page the Character handoff (`design_handoff_character_tk/`) didn't cover. Everything outside the page area stays exactly as it is: the Character and Condition boxes, the Page radiobuttons, the buttons and the status bar. The colours, fonts, relief and radiobutton indicators are the same as in the Character, Items and Knowledge handoffs.

**The main changes:**
- A **Result** column straight after the name, so "am I protected against X?" takes one look instead of adding up thirteen cells.
- **Named slot headings**: the worn item's glyph in its game colour, with the slot's short name under it.
- **One mark per state, each with its own tint**, so `+` always means something good and a drawback can't be mistaken for a benefit.
- **Timed effects get their own column**, so lasting and temporary never share a cell.
- **Help everywhere**: a one-line status-bar hint for whatever is under the pointer, and a card with the full story on a cell, row name, Result or heading.
- The heading card answers **"what do I lose if I take this off?"** directly.
- A **Flags Help window** (640 × 485, tabs plus a scrollbar) explains the page to players. A **Help…** button on the Show row, Help ▸ On Flags… and F1 all open it.

## About the design files
`Flags Tk.dc.html` and `Flags Help Tk.dc.html` are **HTML prototypes**, not production code. Rebuild them in Tcl/Tk with stock widgets. Open them in a browser with `support.js` beside them; the page's **Help…** button opens the help window, and its **Close** comes back. The page's Tweaks switch between the **level 27 sample kit** from the brief and **Telch's real level 1 grid**. Both are drawn at 1:1 in points, so the page area is the real 860 × 340.

## Fidelity
**High fidelity** for layout, marks, colours, behaviour and copy. The grid data is the brief's sample kit. Header values the brief didn't give for level 27 (Gold, Title, Hit points, Experience) show as `—`.

---

## What each question uses

| The player asks | The page answers with |
|---|---|
| 1. Am I protected against X? | The **Result** column, first after the name. |
| 2. What is giving me that? | The row's cells. The row card lists every source and ends with "It's the only source" or "Two things give it, so you could lose one and keep it". |
| 3. What do I lose if I take this off? | The **heading card**: *You'd lose* / *You'd gain* / *No change*. Clicking a heading marks the column so you can read down it. |
| 4. Is it lasting or temporary? | The **Timed** column, the `~` mark, and **For now** in the Result. |
| 5. What is working against me? | The red marks (`−` and `!`), and **Show: Against you**. |
| 6. What don't I know yet? | Grey italic names, `?` cells, **Show: Not yet learned**, and the note shown to a new character. |

---

## Widget tree (Tk)

```
.character.f.page.flags          frame, swapped in and out like the other pages
└─ labelframe .lf  -text "What you have, and what is giving it to you"
    ├─ frame .show    label "Show:" + 4 × radiobutton -variable ::character(flagshow)
    │                 -value all|effect|against|unknown, text "All (55)", "In effect (19)" …
    │                 + button "Help…" packed -side right, -command flags_help (same as F1)
    ├─ frame .note    only while more runes are unlearned than learned:
    │                 label -bitmap info -compound left -text "You've learned 1 of 55 runes. …"
    ├─ frame .grid    -relief sunken -bd 2
    │   ├─ canvas .head  -height 44 -bg #d9d9d9 -highlightthickness 0 -takefocus 0
    │   ├─ canvas .body  -bg white -highlightthickness 0 -takefocus 0
    │   │                -yscrollcommand {.lf.grid.sb set}
    │   └─ scrollbar .sb -command {.lf.grid.body yview}
    └─ frame .legend  per entry: a 18×14 canvas swatch + label
.flagtip                          toplevel, wm overrideredirect 1, -bg black -bd 0
└─ frame -bg #ffffe0, 1 px inside     the card (see Help)
```

**Why canvases:** 16 columns × 55 rows is 880 cells. As labels that is slow to build and to recolour; on a canvas each cell is one rectangle and one text item tagged `cell r$row c$col`, so a refresh is `itemconfigure`. The heading is its own canvas so it stays put while the body scrolls. Nothing scrolls sideways.

## Layout
- **Column tracks**, shared by heading and body: name ≥ 132 px (it takes any spare width), Result 92, then 14 × 44: the slots in the game's order, then **You**, then **Timed**. Build the slot columns from `angband_flags columns`; a shape-changed body has fewer, so never assume 12.
- **Rows** 18 px. **Group bands** 20 px, raised (`#ffffff` top, `#828282` bottom), spanning the full width.
- **Rules:** 2 px `#a3a3a3` before the first slot and before You; 1 px `#ececec` between cells; 1 px `#d0d0d0` before Result.
- **At the default size:** Show row 22, heading 44, legend 18, about 11 rows visible. When the window grows, the grid takes all of the extra height.
- **After the last row**, in `#5a5a5a`: *"Not on this page: your race's and class's own abilities, which nothing can add or take away; mutations and shapes; and a weapon's brands, which belong to its blows and are listed on the weapon."* This keeps what the old four lines of notes said.

## Cell states

Every state has its own mark **and** its own tint, so it reads the same in the colour-blind palette. Take the tints from palette roles (as `character_hp_colour` does with `display_role`) rather than hard-coding them. These are the defaults:

| State | Mark | Ink | Tint | When | Today |
|---|---|---|---|---|---|
| Has it | `+` | `#0b5e0b` | `#d6ecd6` | gives it while worn | `+` |
| Immune | `*` | `#063f06` | `#a9d6a9` | resistances only | `+` (colour, term screen only) |
| Timed | `~` | `#1a3a96` | `#dbe3f7` | Timed column only | `~` under `@`, only if nothing lasting |
| Against you | `−` | `#9c1414` | `#f5d6d6` | vulnerable, or takes it away | `-` |
| Drawback | `!` | `#9c1414` | `#f5d6d6` | Drawbacks only | `+` |
| Not identified | `?` | `#6b4f00` | `#f7ecc4` | see below | `?` across the whole row |
| Nothing | `·` | `#9a9a9a` | white | worn, doesn't give it | `.` |
| Empty slot | none | none | `#e4e4e4` | whole column | `+` (bug) |
| Doesn't apply | none | none | white | e.g. You × Can't be removed | `+` (bug) |
| Modifier | `+2` / `−1` | as Has it / Against you | | signed, no cap | digit, `+` above 9 |

Marks are `{Courier 12 bold}`, centred. **Not identified:** the game knows that an item has unlearned runes but not which ones, so `?` goes in that item's column on **every** row whose rune is unlearned. Putting it on the Nether row alone would give the answer away. In the sample, the Elvenkind shield shows `?` on all seven unlearned rows.

## The Result column
`{Courier 12 bold}` mark, then a word in `{Helvetica 12 bold}` (normal weight for "no"), on the state's tint:

| Group | Words |
|---|---|
| Resistances | `*` Immune · `+` Resist · `~` For now · `?` Unknown · `−` Vulnerable · `·` no |
| Abilities | `+` Yes · `~` For now · `?` Unknown · `−` Blocked · `·` no |
| Drawbacks | `!` Affected · `~` For now · `?` Unknown · `·` no |
| Modifiers | the total, `+7` / `−2`, or `·` |

- A resistance and a vulnerability cancel to **no**. An immunity survives a vulnerability.
- A timed effect adds one level on top of that, and the word becomes **For now**. (Sample: Fire is three resists plus the cursed cloak, which comes to **no**; the potion makes it **For now**.)
- **Unknown** appears only when nothing known gives the property and an unidentified item might.
- The total must be worked out **from known runes only**, or the Result gives away the unidentified property.

## Column headings
44 × 44, raised 1 px, `#d9d9d9`. A black 20 × 20 well (`#5a5a5a` inner edge) holds the worn item's glyph in its game colour, `{Courier 12 bold}`. Under it is the short name in `{Helvetica 11}`: **Weapon, Bow, Ring R, Ring L, Amulet, Light, Body, Cloak, Shield, Helm, Gloves, Boots, You, Timed**. The full slot name and item name go in the status bar and the card.
- **Empty slot:** the slot's symbol in `#3a3a3a`, as on the Items doll.
- **You:** a white `@`. **Timed:** a small hourglass in `#8fb0ff` (two canvas polygons).
- **Pointed at:** `#ececec`. The same happens to the heading of whichever cell is under the pointer, and that row's name gets `#ececec` too, so you can see where you are.
- **Marked** (click): sunken, `#c3c3c3`, with 2 px `#b03060` rails down the column. Click again to clear.

## Rows and bands
- **Learned:** `{Helvetica 12}` black. **Rune not learned:** `#6e6e6e` *italic*, and the row's values are still shown (the brief's bug 2). No more rows that are nothing but `?`.
- **Marked** (click the name): the name on `#c3c3c3`, with 1 px `#b03060` rails along the row.
- **Bands** sum up their group and fold on click (`▼` / `▶`):
  `Resistances  6 of 13 resisted · 1 for now · 3 not known` ·
  `Abilities  5 of 20 · 1 for now · 2 not known` ·
  `Drawbacks  1 affecting you · 2 not known` ·
  `Modifiers  5 raised`. View ▸ Fold All Groups / Unfold All Groups fold or open every band.
- **Show** filters: *All*; *In effect* (the Result isn't no or Unknown); *Against you* (any `−` or `!` cell); *Not yet learned* (the rune is unlearned or a cell shows `?`). Hide a band whose group has no rows left. If nothing at all is left, say so in grey Courier: "Nothing is working against you." / "Nothing is in effect yet." / "You've learned every rune on this page."
- **Legend**, always visible under the grid: `+ has it`, `* immune`, `~ timed`, `− against you`, `! drawback`, `? not identified`, `· nothing`, `empty slot`, *`rune not learned`*. Pointing at an entry explains it in the status bar.

---

## Help

### Part 1: the status bar
Instant, one line, for whatever is under the pointer. When nothing is, it shows: "Point at a row, a cell or a heading to see what it means. Click a heading to mark its column."

| Under the pointer | Pattern | Example |
|---|---|---|
| Cell | `{Row} · {item}: {what it does}{ (detail)}.` | Fire · Cloak of Protection: makes you vulnerable to fire (curse: burning up). |
| You cell | `{Row} · You: {what it does}, {as a Dwarf}.` | Blindness resistance · You: gives you this, as a Dwarf. |
| Timed cell | `{Row} · {effect}: for now. It wears off.` | Fire · Resist heat: for now. It wears off. |
| `?` cell | fixed | Nether · Leather Shield of Elvenkind: not known. It has a rune you haven't learned, and this could be it. |
| Name or Result | `{Row}: {answer}` | Poison: You resist poison. Two things give it, so you could lose one and keep it. |
| Heading | `{Slot}: {item}. Taking it off {loses …, lowers …, improves …, lifts …}.` | Helm: a Metal Cap of Seeing <+3>. Taking it off lowers Searching and lifts No teleportation. |
| Empty heading | `{Slot}: nothing worn.` | Gloves: nothing worn. |
| Band | `{Group}: {summary}. Click to fold.` | Resistances: 6 of 13 resisted · 1 for now · 3 not known. Click to fold. |

### Part 2: the card
A small borderless toplevel: `#ffffe0`, 1 px black border, `{Helvetica 12}`.
- Shows **450 ms** after the pointer settles on a cell, row name, Result or heading. While one is up, moving to the next cell updates it **at once**. Bands, legend and Show get the status bar only.
- Hides when the pointer leaves the grid, and on scroll, click or any key (keys still go to the game).
- Placed 16 px right of and 20 px below the pointer; flips left or up at the window's edge.

**Row card**, on a cell, name or Result:
1. **Title** (bold) on the left, the group on the right in `#5a5a5a`.
2. **Note** in italic, only for an unlearned rune: "You haven't learned this rune yet. You'll recognise it on an item the first time you notice it."
3. **Description**, from the game data (copy below).
4. **Sources**, one line per cell that isn't nothing, empty or doesn't-apply: mark swatch · slot · item (curse in brackets). The line for the cell under the pointer is bold on `#f0e08c`.
5. **Answer**, after a rule: the Result in words. For one source, name it and add "It's the only source." For two or more, "{Two} things give it, so you could lose one and keep it." For the cancelled case: "Resisted for now, by Resist heat. Your Cloak of Protection makes you vulnerable, which cancels the resistance from the other three, so once it wears off you won't resist fire."

**Column card**, on a heading:
1. **Title:** `{Slot}: {full item name}`. In italic: "Cursed: {curse}." and/or "It has runes you haven't learned."
2. "If you take it off:" then lines of *You'd lose* (`−`), *You'd gain* (`+`), *No change* (`=`, one line listing them), and *Unknown* (`?`) "whatever it has that you haven't learned yet". Format: `Shards: resist → no`, `Searching +6 → +3`, `No teleportation: affected → no`.
3. If nothing changes: "Taking it off would change nothing here: other things you wear cover all of it." (In the sample that is the Body armour and the Ring of Resist Poison.) Last line: "Click to mark this column; double-click to inspect the item."
- **You:** "What your race, class and level give you. You can't take these off." and the list. **Timed:** "What potions, scrolls and spells are giving you right now. They wear off on their own." and the list. **Empty:** "Wear something here and this column fills in."

To build the card, work out each row's Result twice: once as it is and once without that column, then compare them (resistances: Vulnerable < no = Unknown < For now < Resist < Immune).

### Part 3: the Flags Help window
The status bar and the cards explain one thing at a time. The help window explains the page as a whole, for a player who wants to read about it. It is sized for a laptop: **640 × 485** by default, resizable, minimum 480 × 360.

```
.flagshelp                 toplevel -title "Zangband: Flags Help", wm minsize 480 360
├─ ttk::notebook .nb       ttk::notebook::enableTraversal .nb  (Ctrl+Tab, Alt+underlined letter)
│   └─ one frame per tab:  text -wrap word -state disabled -bg white -padx 14 -pady 10
│                          -font {Helvetica 12} -spacing3 6  +  scrollbar
└─ frame .btm              label (one-line summary of the tab, normal weight)  <spacer>
                           button "Close" -default active -underline 0
```

| Tab | What it covers |
|---|---|
| The **G**rid | What rows, columns and Result are. A small example grid (an embedded canvas, drawn by the same cell code) showing the cancelled-fire case. How to get an explanation. Show, folding, marking, double-click to inspect, F1 and Esc. |
| **M**arks | Every cell mark, with its swatch, name and meaning in player words. Swatches are text tags (`-background`, `-foreground`, `{Courier 12 bold}`). |
| **R**esults | The words each group's Result can say, and the three rules: a vulnerability cancels a resistance; For now; Unknown. |
| **C**olumns | The slots in order, empty slots, You and Timed, and how to use the heading card to decide what to take off. |
| **L**earning | Runes: grey names, why `?` is on every unlearned row, and why a new character's page starts out grey. |
| **P**roperties | Every row's description, grouped like the page. Built from the same data as the cards, so the two can't disagree. |

- One window at a time: F1 or Help… again raises it. It remembers its tab and size while the game runs.
- It isn't modal, so it can stay open beside the Character window. Esc or Close closes it.
- Optional: F1 while pointing at a row opens **Properties**, scrolled to that row.
- The prototype shows "[Description to come.]" against rows that have no copy yet, so the gaps are visible. In the game, leave those rows out until they're written.
- The big reference sheet on the design canvas is for whoever builds this. It is not a window in the game.

### Help copy for the rows in the sample
Second person; say what it does for you; one or two short sentences. Where the data has only a fragment ("fear", "blindness"), use "Protects you from {fragment}."

| Row | Description |
|---|---|
| Acid | Acid attacks hurt you less, and are less likely to damage what you carry. |
| Lightning | Lightning attacks hurt you less, and are less likely to destroy what you carry. |
| Fire | Fire attacks hurt you less, and are less likely to burn what you carry. |
| Cold | Cold attacks hurt you less, and are less likely to shatter what you carry. |
| Poison | Poison attacks hurt you less, and can't leave you poisoned. |
| Light | Bright light hurts you less, and can't blind you. |
| Darkness | Darkness attacks hurt you less, and can't blind you. |
| Sound | Sound attacks hurt you less, and can't stun you. |
| Shards | Shard attacks hurt you less, and can't cut you. |
| Nexus | Nexus attacks hurt you less, and can't teleport you, pull you to a monster or swap your stats around. |
| Nether | Nether attacks hurt you less, and can't drain your experience. |
| Chaos | Chaos attacks hurt you less, and can't confuse you, make you see things or drain your experience. |
| Disenchantment | Disenchantment can't strip the bonuses from your equipment. |
| Pass wall | You can walk through solid rock. |
| Cut resistance | Protects you from cuts. |
| Fear resistance | You can't be frightened, so you can always fight back. |
| Blindness resistance | You can't be blinded, so you can still read scrolls and see what's around you. |
| Confusion resistance | You can't be confused, so you can still read scrolls and walk where you mean to. |
| Stun resistance | You can't be stunned or knocked out. |
| Hold life | Protects your experience from being drained. |
| Regeneration | You recover hit points and mana faster, but get hungry faster too. |
| Telepathy | You sense the minds of nearby monsters, even through walls. Mindless monsters stay hidden. |
| See invisible | You can see invisible monsters, such as ghosts, when they are in view. |
| Free action | You can't be paralysed. Deeper down, monsters that paralyse with a touch or a spell are among the most dangerous. |
| Feather falling | You drift down trapdoors and into pits instead of falling, and take no damage. |
| Slow digestion | You get hungry more slowly. |
| Trap immunity | Traps can't hurt you. |
| Bless | Blessed: your armour class and chance to hit are raised. |
| Slow healing | You recover hit points more slowly. |
| Slow mana recovery | You recover mana more slowly. |
| Afraid | You are always afraid, so you can't fight in melee. |
| Aggravation | Wakes nearby monsters and makes them angry. |
| No teleportation | You can't teleport: Phase Door, Teleportation and the like won't work on you. |
| Drains experience | Slowly drains your experience while you wear it. |
| Can't be removed | Once it's on, you can't take it off until the curse is broken. |
| Stealth | How quietly you move. The higher it is, the less often monsters wake up. |
| Searching | How readily you notice traps, secret doors and hidden things. |
| Infravision | Lets you see warm-blooded monsters in the dark, 10 ft for each point. |
| Tunneling | How quickly you dig through rubble, mineral veins and rock. |
| Speed | How often you act. At +10 you act twice as often as a normal-speed monster. |
| Light radius | How far your light reaches, in squares. |

**Still to write**, because the mechanics are this variant's own and I didn't want to guess: Luck, Electric aura, Fiery aura, Ghoul touch, Reflection, Cannot eat, Anti-magic, Fragile, Mana, Attack speed, Shooting speed, Shooting power, Damage reduction, Movement speed. Please also check the resistance copy against this game's projection data. It describes the usual Angband side effects. Bless assumes the row is the timed blessing; if it also covers blessed weapons, it needs a second sentence.

## Label changes
The labels come from the data files, so these are cheap:

| Today | Proposed | Why |
|---|---|---|
| Elec | Lightning | Plain word |
| Dark | Darkness | Plain word |
| Shard | Shards | Plain word |
| Disenchant | Disenchantment | It is a noun everywhere else |
| Prot. cut | Cut resistance | Matches Fear / Stun resistance |
| Blindess resistance | Blindness resistance | Typo (bug 3) |
| ESP | Telepathy | Says what it is |
| *Hindrances* (group) | Drawbacks | Reads as bad at a glance |
| Impaired HP recovery | Slow healing | Fits the column; plainer |
| Impaired mana recovery | Slow mana recovery | Fits the column |
| Fear | Afraid | Not confused with Fear resistance |
| Aggravate | Aggravation | A state, like the others |
| Drain experience | Drains experience | Says the item does it |
| Sticky curse | Can't be removed | Answers question 3 directly |
| Search | Searching | Plainer |
| Light (modifier) | Light radius | There is also a Light resistance |

---

## ⚠ Data to confirm
1. **Unlearned runes per slot.** For the `?` cells, `angband_flags columns` needs a flag saying whether each worn object has runes the character hasn't learned (`object_runes_known`). It is not in the bridge today.
2. **Curse names.** "(curse: burning up)" needs each cell to say whether its value comes from a curse, and which one. Without that, drop the brackets; the item's "Cursed: …" line in the column card still shows.
3. **You, split up.** "as a Dwarf" needs the character's column split into race, class and level. Without it, say "from your race, class or level".
4. **Timed names.** The Timed column needs the effect's own name ("Resist heat", "Blessing") per row, not just the aux value. Turns left would be a welcome extra ("Resist heat, 212 turns").
5. **Totals.** The brief says the game computes these. They must count known runes only (see the Result column). For the column card, either ask the game for the total without slot N, or recompute in Tcl with the rule above.
6. **Item short names** (no article, numbers or inscription: "Cloak of Protection") for the status bar and the source lines. The full name goes in the column card title.

## Interactions & behaviour
| Action | Result |
|---|---|
| Pointer over a cell, name, Result or heading | Status-bar line now; card after 450 ms. That row's name and that column's heading get `#ececec`; the cell gets a 1 px black inner ring. |
| Pointer over a band, legend entry or Show button | Status-bar line only. |
| Pointer leaves the grid, or scroll | Hide the card, restore the status bar. |
| Click a row name | Mark or unmark the row. |
| Click a heading | Mark or unmark the column. |
| Double-click a heading | Inspect that item, as the Items window's Inspect does. |
| Click a band, or View ▸ Fold All / Unfold All | Fold or unfold. |
| Show radiobutton | Filter the rows; the counts in brackets don't change. |
| Help… button, Help ▸ On Flags…, or F1 | Open (or raise) the Flags Help window on its last tab. |
| Mouse wheel over the grid | Scroll the body. |
| Any key | Hides the card and goes to the game as now. The canvases take no focus. Esc closes the window. |

```tcl
foreach w [list $head $body] {
    bind $w <Motion> [list flags_hover $w %x %y %X %Y]
    bind $w <Leave>  flags_tip_hide
}
bind $body <1>          [list flags_click_row %x %y]
bind $head <1>          [list flags_click_col %x]
bind $head <Double-1>   [list flags_inspect %x]
bind $body <MouseWheel> {%W yview scroll [expr {-%D / 120}] units ; flags_tip_hide}
bind .character <Key>   {+ flags_tip_hide}
```

## State
```tcl
# Kept while the window is open, like ::character(page).
set ::character(flagshow) all          ;# all | effect | against | unknown
# ::character(flagfold,res|abil|draw|mod)  0 or 1
# ::character(flagrow)  row id or ""     ::character(flagcol)  column index or ""
# Everything else comes from angband_flags; redraw on the window's existing refresh.
```

## Assumes the brief's three fixes
Empty slots no longer read as "has it"; unlearned rows show their values with a dimmed name; "Blindess" is spelled correctly.

## Prototype-only extras (do not implement)
- The WM frame and the checkered desktop.
- The **Tweaks** switch between the two sample characters.
- The other pages are stubs here; they are unchanged and in `design_handoff_character_tk/`.
- `—` in the header where the brief gave no value.

## Files
- `Flags Tk.dc.html`: the interactive page prototype (needs `support.js` beside it).
- `Flags Help Tk.dc.html`: the interactive help window prototype.
- `support.js`: the prototype runtime, the same file as in the other Tk handoffs.
- `after-1-level27.png` … `after-6-level1.png`: the new page, from the prototype.
- `after-7-help-grid.png`, `after-8-help-properties.png`: the help window.
- `before-flags-1-top.png`, `before-flags-2-abilities.png`, `before-flags-3-bottom.png`: the page as it is today.
