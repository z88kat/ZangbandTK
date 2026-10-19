# Brief: the Flags page of the Character window (classic Tk)

## What we're asking for

Please redesign the **Flags** page of the Character window.

It's the one page your `design_handoff_character_tk` didn't cover. It was added in code afterwards as a block of monospaced text, and it shows. Steven's verdict: it's hard to read and hard to use, and with no help or tooltips you can't tell what it's telling you.

Keep everything around the page as you designed it: the Character and Condition boxes, the Page radiobuttons, the buttons and the status bar. Only the page area changes. Use the same visual system as your Knowledge, Character and Items handoffs (classic Tk on X11, `#d9d9d9` ground, `{Helvetica 12 bold}` labels, `{Courier 12}` for data, the same relief and bevels).

Please deliver it the way you delivered the others: a README, a `.dc.html` prototype with `support.js`, and these "before" screenshots.

## The screenshots

| File | Shows |
|---|---|
| `before-flags-1-top.png` | Top of the page: the column headings, then the resistances |
| `before-flags-2-abilities.png` | Scrolled down: the abilities and the start of the hindrances |
| `before-flags-3-bottom.png` | Bottom: the modifiers, then the legend and the notes |

The character is Telch, a level 1 Dwarf Warrior wearing three things: a weapon, a torch and soft leather armour.

---

## What the page is for

A player opens it to answer these questions. They're listed roughly by how often people ask them.

1. **Am I protected against X?** Before going deeper: do I resist poison? Do I have free action? Can I see invisible monsters?
2. **What is giving me that?** Which worn item, or the character themselves (race, class, or a gift that came with a level). This decides whether a ring can be swapped for a better one without losing something.
3. **What do I lose if I take this off?** The same grid, read down one column.
4. **Is it lasting or temporary?** A potion of Resist Heat lasts a few hundred turns. A ring lasts until you take it off.
5. **What is working against me?** Curses and drawbacks on what I'm wearing.
6. **What don't I know yet?** Properties the character hasn't learned to recognise. In this game you learn each property (a "rune") the first time you notice it, and until then the game won't tell you whether an item has it.

The current page contains the material for questions 2 to 6 but never answers question 1 directly. You have to add up a row of thirteen cells in your head.

## What's wrong with it now

1. **The column headings are a row of single glyphs:** `|----~(-----@`. There's one character per equipment slot (the worn item's symbol, or `-` when the slot is empty), then `@` for the character. Nothing says which slot is which. With seven identical dashes there's no way to find "left hand".
2. **There's no total.** To find out whether you resist fire you have to read thirteen cells. The game already works out the combined answer, including the rule that a resistance and a vulnerability cancel each other out. The page just doesn't show it.
3. **A new character sees a wall of `?`.** Telch has 55 rows and 54 of them are entirely question marks, so the page looks broken. Part of this is a bug (see "Known bugs" below), but even with the bug fixed, an early character will have many unlearned rows.
4. **The legend is at the very bottom,** under about 60 lines, in grey. The last two notes run off the right edge ("nothing can grant or tak…").
5. **Labels run into the grid.** Labels are padded to 20 characters with no gap after them. "Confusion resistance", "Impaired HP recovery" and "Impaired mana recovery" are 20 characters or more, so the first cell touches the last letter.
6. **It's one block of monospaced text.** There are no column separators and no row grouping beyond a bold heading. Nothing can be hovered or selected. Thirteen columns by 55 rows is about 700 single characters.
7. **`+` means opposite things in different groups.** Under Resistances and Abilities, `+` is good. Under Hindrances, `+` means "you have this drawback". The mark and the colour are the same either way.
8. **Modifiers lose their number.** Anything above +9 is shown as `+`, so boots of speed +10 and +15 look the same.
9. **There's no help anywhere.** No tooltip, no status-bar hint, and no explanation of terms like Nexus, Hold life or Free action, or of why a row is all `?`.

---

## The data

Everything below exists in the game today or can be computed from what exists. Mark anything else you'd want with ⚠ and we'll say whether it's possible.

### Columns

There are thirteen: one per equipment slot, in the game's order, then the character. Please use the slot names from your Items handoff:

| # | Slot | Telch wears |
|---|---|---|
| 1 | Weapon | a Dagger `\|` |
| 2 | Bow | (empty) |
| 3 | Ring (right hand) | (empty) |
| 4 | Ring (left hand) | (empty) |
| 5 | Amulet | (empty) |
| 6 | Light | a Wooden Torch `~` |
| 7 | Body | Soft Leather Armour `(` |
| 8 | Cloak | (empty) |
| 9 | Shield | (empty) |
| 10 | Helm | (empty) |
| 11 | Gloves | (empty) |
| 12 | Boots | (empty) |
| 13 | **You** | race, class and level gifts (shown as `@` today) |

For each slot column the game can provide the slot name, the worn item's full name, its glyph and its colour, or "empty".

The number of slots comes from the character's body. A normal body has 12. Design for 12, but don't build the layout on exactly 12: a shape-changed character can have fewer.

**Temporary effects** (potions, scrolls, spells) are a separate channel. Today they're only visible as `~`, and only when nothing permanent gives the same thing. The game knows both separately, so you can show both.

### Rows

There are four groups and 55 rows. Both the order and the labels come from the game's data files, so changing either is cheap. Please do suggest better wording where a label is unclear.

| Group | Rows |
|---|---|
| **Resistances** (13) | Acid, Elec (lightning), Fire, Cold, Poison, Light, Dark, Sound, Shard, Nexus, Nether, Chaos, Disenchant |
| **Abilities** (20) | Luck, Electric aura, Fiery aura, Ghoul touch, Pass wall, Reflection, Prot. cut, Fear resistance, Blindness resistance, Confusion resistance, Stun resistance, Hold life, Regeneration, ESP (telepathy), See invisible, Free action, Feather falling, Slow digestion, Trap immunity, Bless |
| **Hindrances** (10) | Cannot eat, Anti-magic, Impaired HP recovery, Impaired mana recovery, Fear, Aggravate, No teleportation, Drain experience, Sticky curse, Fragile |
| **Modifiers** (12) | Mana, Stealth, Search, Infravision, Tunneling, Speed, Attack speed, Shooting speed, Shooting power, Light, Damage reduction, Movement speed |

**Help text.** The game data has a description for most properties, but the descriptions are uneven. Some are sentences ("Grants the ability to see invisible things"), and some are fragments meant to be dropped into a sentence ("fear", "blindness"). Please design the *pattern* for help (what a tooltip or status-bar hint says, and where it appears), and write the copy for the rows in your sample. We'll write the rest to match.

### What a cell can say

**Resistances, Abilities and Hindrances:**

| State | Meaning | Shown today as |
|---|---|---|
| None | This slot doesn't give it | `.` |
| Has it | Permanent | `+` |
| Takes it away | The item removes or opposes it | `-` |
| Temporary only | From a timed effect, with nothing permanent | `~` |
| Permanent and temporary | Both at once | `+` (the temporary part is hidden) |
| Empty slot | Nothing is worn there | `+`, which is a bug (see below) |
| Can't apply | e.g. the You column on a row only items can have, such as Sticky curse | `+`, the same bug |
| Not identified | The item may have it, but the character hasn't learned to recognise it yet | ⚠ not shown separately today |

**Resistances have extra levels:** resist; immune (stronger than resist); vulnerable (takes extra damage); and resist plus vulnerable together, which cancel out to nothing. The game's text-mode screen tells these apart by colour.

**Modifiers** are a signed number per cell (`+2`, `-1`) and a total. A typical range is -5 to +15. Speed can go higher.

**Whole rows.** Each row also has a "rune known" flag. In the game's own text-mode screen, an unlearned rune only dims the row label, and the values still show. The Tk page currently blanks the whole row to `?` instead. That hides things the character certainly knows about themselves: Telch's dwarven Infravision +5 and Tunneling +2 are both behind question marks. We'll change it to match the game, so please design for a dimmed or marked row label, with `?` only in the individual cells where a worn item's property is unidentified.

**Totals.** The game computes a combined result for every row (for example "Fire: not resisted" or "Speed: +7"). The page doesn't show it today, but it's available.

### Sample data for a populated grid

Telch's real grid is nearly empty, and none of Steven's saved characters is past the early game. So here is a mid-game kit built from real item definitions in the game. The properties are the game's own. The numbers are within each item's real range. Use this for the main state of the prototype, and Telch's real grid (the screenshots) for the new-character state.

**Telch at level 27, a Dwarf Warrior, 1,250 ft down:**

| Slot | Item | What it puts on this page |
|---|---|---|
| Weapon | a Broad Sword (2d5) (+8,+10) (Defender) <+2> | resist acid, elec, fire, cold · free action · see invisible · feather falling · regeneration · stealth +2 |
| Bow | a Light Crossbow (x3) (+5,+6) | nothing on this page |
| Ring (right hand) | a Ring of Resist Poison | resist poison |
| Ring (left hand) | a Ring of Digging <+4> | tunneling +4 |
| Amulet | an Amulet of Trickery | resist nexus · resist poison (a second source) · stealth +2 · search +3 · infravision +2 · speed +1 |
| Light | a Lantern (7500 turns) | nothing (its light radius is on the Info page) |
| Body | Metal Scale Mail of Resistance [38,+12] | resist acid, elec, fire, cold |
| Cloak | a Cloak of Protection [1,+8], **cursed** ("burning up") | resist shard · resist cold · **vulnerable to fire** (from the curse) |
| Shield | a Leather Shield of Elvenkind [8,+9] <+1> | resist acid, elec, fire, cold · stealth +1 · **one more resistance not yet identified** (it's nether; the Nether rune is unlearned) |
| Helm | a Metal Cap of Seeing <+3>, **cursed** ("anti-teleportation") | blindness resistance · see invisible · search +3 · **No teleportation** (a hindrance) |
| Gloves | (empty) | |
| Boots | a Pair of Leather Boots of Speed <+6> | speed +6 |
| **You** | Dwarf Warrior | blindness resistance · infravision +5 · tunneling +2 |

**Active temporary effects:** a Potion of Resist Heat (fire, temporary) and a Scroll of Blessing (Bless, temporary).

**Unlearned runes:** Nether, Chaos, Disenchant, Pass wall, Ghoul touch, Anti-magic, Drain experience.

This kit deliberately includes the cases the design has to handle:

- **Fire cancels out.** Three items resist fire and the cursed cloak makes you vulnerable. By the game's rule a resistance plus a vulnerability is no resistance, so the answer to "do I resist fire?" is **no**, until the potion is counted, which makes it **yes, for now**. A row of `+ + + -` doesn't say that. A total does.
- **Redundancy.** Resist poison comes from both the ring and the amulet, so the ring could be swapped for something else. Blindness resistance comes from both the helm and the character.
- **A hard trade-off.** The helm gives see invisible and blindness resistance, but its curse stops you teleporting. That's the "can I take this off?" question with a real cost on both sides.
- **An unidentified property** in one cell (Shield × Nether) on an unlearned row.
- **Modifiers that add up:** Stealth +2 +2 +1 = +5, Speed +1 +6 = +7, Infravision +2 +5 = +7, Tunneling +4 +2 = +6.
- **An empty slot** (Gloves).

---

## Constraints

- **Stock Tk widgets,** as in your other handoffs. A canvas or a grid of labels both work for a 14-by-55 table, and per-cell colour, hover and click are all fine.
- **Tooltips have to be hand-made** in Tk (a small borderless toplevel on `<Enter>`). That's fine, but keep the behaviour simple. Status-bar hints cost nothing, since the window already has one.
- **A sticky header** (column headings that stay put while the rows scroll) is possible.
- **Size.** The window is about 920 × 730 at normal resolution, and the page area shows about 860 × 340 before it scrolls. Thirteen slot columns plus labels and a total have to fit across that, so slot names will need abbreviating, stacking or rotating. That's your call.
- **Don't rely on colour alone.** The game has a colour-blind palette option, so every state needs a different mark as well as a different colour.
- **Keys typed in the window still play the game,** Esc closes it, and the page is read-only.

## Ideas we'd like you to consider

These are suggestions, not requirements.

- A **total or result column** placed first, so question 1 is answered without reading across.
- Column headings that **name the slot** and show the worn item's glyph, with the full item name on hover.
- **Hovering a cell** says, in a tooltip or the status bar, what it means and where it comes from: "Fire — your Cloak of Protection makes you vulnerable (curse: burning up)".
- **Hovering or selecting a row** explains the property and lists every source, ending with the combined result.
- Hindrances that **read as bad**, so a drawback can't be mistaken for a benefit.
- A **legend that's always visible**, not buried at the bottom.
- A **filter**, such as "All / Only what I have / Not yet known", or collapsible groups, so 55 rows aren't all on screen at once.
- A **new-character state** that explains why so much is unknown instead of filling the page with question marks.
- Clicking a column heading could open that item's full description (the Items window has an Inspect action).

## Known bugs, which we'll fix (please don't design around them)

1. **Empty slots show `+`.** In `before-flags-2-abilities.png`, Blindness resistance reads `.++++..++++++`, as if Telch's empty ring, amulet and boot slots resisted blindness. The game marks an empty slot with a special "not present" value, and the page misreads it as "has it". The correct row for Telch is `+` in the You column only, because dwarves resist blindness.
2. **Whole rows show `?` when the rune is unlearned.** The game's own screen dims the label and still shows the values (see "Whole rows" above).
3. **"Blindess resistance"** is a typo in the game data.

## What we're not asking for

- Changes to the other pages, the header boxes or the buttons.
- Anything the game doesn't have. If you want something that isn't listed above, mark it ⚠.
