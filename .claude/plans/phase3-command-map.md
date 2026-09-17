# The old commands, and what each one becomes

*A T3 deliverable. §3.3 of [the Phase 3 plan](phase3-tcl-tk-frontend.md) sets the rule this
document applies: **keep the old command names wherever 4.2 has an equivalent concept**, and
rename only where the concept genuinely differs. This is the list that turns that rule into
work somebody can estimate.*

## How this was measured

Call sites and script counts come from the original's own Tcl, counting `angband <command>`
across `archive/Tk/OmnibandTk-1.4/tk` and `archive/Tk/CommonTk-1.4/OmnibandTk/tk` — 2,825
calls across 32 commands. The 4.2 column is a specific symbol in this tree, checked to exist
rather than assumed; where it does not exist, the row says so.

Script counts are of distinct file names across both trees, which overlap: §3.3's table was
counted against one of them and reads a little lower for the same commands. Where the two
disagree, this one is the wider count and the difference is the same script appearing in both
distributions, not extra work.

The verdicts are four:

- **Kept** — 4.2 has the same concept under the same name. The scripts keep working once the
  accessor is written.
- **Renamed** — 4.2 has the concept somewhere else. Rename deliberately, once, and fix the
  scripts.
- **Seam** — replaced by one of §3.2's four seams. These scripts change no matter what the
  command is called, because what they *do* is being replaced. Exempt from the naming rule.
- **Dropped** — Zangband-only, or a front-end internal with nothing behind it. Nothing to
  write and nothing to port.

## The map

| Old command | Calls | Scripts | 4.2 concept | Verdict |
|---|---:|---:|---|---|
| `keypress` | 479 | 32 | the command queue — `cmdq_push`, `angband_push` | **Seam** |
| `player` | 434 | 43 | [`struct player`](../../src/player.h) | Kept |
| `sound` | 286 | 9 | [`sound-core.c`](../../src/sound-core.c) | **Seam** |
| `k_info` | 207 | 15 | [`k_info`](../../src/object.h#L251), `struct object_kind` | Kept |
| `r_info` | 178 | 15 | [`r_info`](../../src/monster.h#L476), `struct monster_race` | Kept |
| `inkey_flags` | 165 | 18 | the input hooks — `angband_hook` | **Seam** |
| `store` | 152 | 5 | [`struct store`](../../src/store.h) | Kept |
| `setting` | 118 | 9 | the option table — `angband_option` | **Seam** |
| `inventory` | 98 | 10 | `player->upkeep->inven` | Kept |
| `info` | 94 | 10 | [`object-info.c`](../../src/obj-info.c) | Kept |
| `spell` | 72 | 6 | [`player-spell.h`](../../src/player-spell.h) | Kept |
| `game` | 70 | 11 | [`game-world.h`](../../src/game-world.h) — `turn`, `character_dungeon` | Kept |
| `cave` | 58 | 7 | [`struct chunk`](../../src/cave.h) | Kept |
| `o_list` | 49 | 8 | `cave->objects`, [`cave.h`](../../src/cave.h#L218) | **Renamed** |
| `mindcraft` | 44 | 6 | *nothing* — a Zangband class | **Dropped** |
| `f_info` | 40 | 7 | [`f_info`](../../src/cave.h#L135), `struct feature` | Kept |
| `power` | 30 | 5 | *nothing* — Zangband mutations | **Dropped** |
| `a_info` | 30 | 3 | [`a_info`](../../src/object.h#L318), `struct artifact` | Kept |
| `m_list` | 29 | 5 | `cave->monsters`, [`cave.h`](../../src/cave.h#L221) | **Renamed** |
| `message` | 28 | 3 | [`message.h`](../../src/message.h) | Kept |
| `equipment` | 26 | 5 | `player->body.slots` | Kept |
| `system` | 24 | 11 | Tcl's own `platform`, `file`, `clock` | **Dropped** |
| `keymap` | 22 | 5 | [`ui-keymap.h`](../../src/ui-keymap.h) | **Seam** |
| `home` | 20 | 2 | `stores[STORE_HOME]` | **Renamed** |
| `macro` | 16 | 1 | *nothing* — 4.2 replaced macros with keymaps | **Dropped** |
| `building` | 14 | 1 | *nothing* — Zangband town buildings | **Dropped** |
| `inkey_other` | 10 | 4 | the input hooks | **Seam** |
| `init_icons` | 10 | 4 | `grafmode.h`, already done in T2 | **Dropped** |
| `highscore` | 8 | 1 | [`score.h`](../../src/score.h) | Kept |
| `floor` | 8 | 2 | `square_object()`, [`cave.h`](../../src/cave.h) | **Renamed** |
| `equipinfo` | 4 | 1 | folded into `equipment` | **Dropped** |
| `keycount` | 2 | 1 | `cmd_get_nrepeats()` | **Renamed** |

## What the numbers say

**Kept names cover 1,495 calls — 53%,** across fourteen commands. This is the payoff from the
naming rule, and it is the largest column by some distance: those scripts are adapted rather
than rewritten, and the work is writing fourteen accessor families instead of re-deriving
what 108,000 lines of Tcl should have said.

**Seams cover 1,080 calls, 38%.** The command queue, the input hooks, the option table and
the keymap API between them account for well over a third of every call the original's
scripts make into C — and all four are built. None of that has to be written twice.

**Dropped is 142 calls, 5%,** and it is two things: a Zangband system 4.2 does not have
(`mindcraft`, `power`, `building`, 88 calls between them) or a front-end internal already
replaced (`system`, `macro`, `init_icons`). Nothing here is a loss. `power` and `mindcraft`
would have to be reintroduced to the *game* first, which is a question for the Amber work and
not for Phase 3.

**Renamed is the smallest column at 108 calls, 4%,** and four of its five rows are the same
rename: 4.2 keeps the level's objects and monsters inside `struct chunk` rather than in
global arrays beside it. `o_list`, `m_list`, `floor` and `home` all become reads through
`cave` or `stores`. One decision, applied five times, not five decisions.

## The order this suggests

The milestones in §5 were written before these numbers existed. They survive them, with one
observation: `player` alone is 434 calls across 43 scripts — a fifth of everything, and more
scripts than any other command touches. **T4 is correctly first**, and it is larger relative
to the others than the plan's milestone list makes it look.

`k_info`, `r_info`, `f_info` and `a_info` are 455 calls — 30% of the Kept column — across
four families that are all the same shape: a flat array of game data, read-only, indexed by a
number the scripts already hold. They are the cheapest large thing left, and they all land in
T6.
