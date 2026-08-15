# zconv — Zangband → Angband 4.2 data conversion

Converts Zangband 2.7.5 game data onto Angband 4.2's data model, implementing the
requirements in `.claude/plans/phase1-balance-calibration.md`.

**The review report is the deliverable, not the data files.** The data files are a side
effect of an auditable process (BAL-11). Read the report's *Invented values* and *Unmapped
flags* sections before accepting anything it generates.

## Usage

```sh
cd tools/zconv

./zconv.py analyse                  # derive scales, write nothing
./zconv.py monsters                 # convert; report only
./zconv.py monsters --write         # convert; write the data file too
./zconv.py monsters --theme-only    # DEC-19: Amber/Mythos/Chaos identity only
./zconv.py artifacts --write        # convert a_info.txt -> 4.2 artifacts
./zconv.py egos --write             # convert e_info.txt -> 4.2 ego items
```

Output lands in `out/` — `monsters.report.md` and `monster.zangband.txt`.

Requires Python 3.11+ (for `tomllib`). No third-party dependencies.

## What it implements

| Requirement | Behaviour |
|---|---|
| **BAL-02** | Divides Zangband `mexp` by monster level. Zangband stores *total* experience and awards `mexp / plev`; 2.8.1 and 4.2 award `mexp * level / plev`. Importing without this inflates awards ~20×. |
| **BAL-05** | Rescales armour class ×9/8. 2.8.1 and Zangband roll against `ac * 3/4`; 4.2 rolls against `ac * 2/3`. |
| **BAL-06** | Converts `NdM` hit dice to 4.2's average-hp integer. |
| **BAL-07** | Maps `sleep` → `sleepiness` using a relationship recovered empirically from the 434 monsters 2.8.1 and 4.2 share. |
| **BAL-09** | Places monsters 4.2 has never seen onto 4.2's own depth curve, using Zangband's numbers only as a *relative* signal of role. |
| **BAL-10** | Reports every source flag with no 4.2 equivalent. Never drops one silently. |
| **BAL-11** | Emits a review report naming, for each value: the rule that produced it, a confidence level, and whether it was invented. |
| **BAL-12** | Deterministic and re-runnable. Hand edits live in `overrides.toml` and are reapplied last, so a re-run never discards tuning. |
| **BAL-13/14** | Reads the lethality scalars from `constants.txt` when present, so calibration never silently assumes 1.0. |

## Confidence levels

Every converted field carries one. Anything below `converted` wants human eyes.

- `exact` — same meaning and scale in both versions; taken unchanged
- `converted` — transformed by a verified rule (a scale or units change)
- `derived` — computed from 4.2's own curve; plausible but unverified
- `invented` — no basis in either source; the tool guessed

## Companion mapping files

Three data files record how Zangband's vocabulary translates to 4.2's. Keeping
them as data rather than code means every decision is reviewable in one place,
and BAL-10's "never silently dropped" rule is enforced by the tool.

| File | Covers |
|---|---|
| `flagmap.toml` | Every monster flag with no 4.2 name: renamed, converted to a field, rejected, deferred to a milestone, or queued for implementation. |
| `basemap.toml` | Glyphs whose 4.2 `monster_base` template lives under a different display character. |
| `blowmap.toml` | Blow methods and effects outside 4.2's `blow_methods.txt` / `blow_effects.txt` vocabularies. |
| `objflagmap.toml` | Object flags, split across 4.2's `flags:`, `values:`, `brand:`, `slay:` and `curse:` destinations. |
| `basesubs.toml` | Base objects 4.2 retired, and the nearest surviving substitute. |
| `slotmap.toml` | Zangband ego equipment slots, expanded to 4.2's object types. |

## Findings so far

**BAL-07 is resolved: the sleepiness scale did not change.** Across 434 shared monsters the
fitted ratio is exactly 1.0000, and most values map to themselves (5→5, 10→10, 20→20, 40→40,
50→50, 60→60, 70→70, 80→80, 99→99, 120→120). The handful of deviations are individual
monsters 4.2 retuned, not a rescale. 4.2's "0–255" documentation describes the field's range,
not a conversion from 2.8.1's. **No conversion is needed** — this closes balance-calibration
open question 1.

**38 monsters use glyphs 4.2 has no `monster_base` template for.** Zangband has monster
classes vanilla lacks: `*` floating spheres (ball lightning, freezing sphere), `|` animated
weapons (death sword), `(` animated armour (cloaker), `.` space monster. `base:` is mandatory
— all 624 of 4.2's own monsters carry one — so these currently receive a placeholder and are
flagged as invented. **New `monster_base.txt` templates are needed**, which is CNT-04 work.

**62 distinct Zangband flags have no 4.2 equivalent**, led by `FORCE_MAXHP` (221 monsters),
`DROP_CORPSE` (140), `CAN_FLY` (117), `RES_TELE` (102), `DUN_CAVERN` (99) and `CAN_SWIM` (91).
Each needs mapping, implementing, or explicit rejection per BAL-10.

## Layout

```
zconv.py        CLI entry point
zformat.py      reader for 2.8.1-era N:/I:/W:/F: files
aformat.py      reader and writer for 4.2 key:value gamedata
rules.py        conversion rules, each citing its requirement
report.py       the review report
overrides.toml  hand-tuned values, reapplied every run
out/            generated output (not committed)
```

## Extending

Adding a data file means a reader interpretation in `zformat.py`, an emitter in the CLI, and
rules in `rules.py`. Per **BAL-08**, check the consuming formula in *both* codebases before
converting any numeric field — a field comparison without a formula check is what produced
the 20× experience error this tool now exists to prevent.
