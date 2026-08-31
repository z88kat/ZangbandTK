# Phase 1 Requirements — Balance Calibration

**Status:** draft, revision 2 · **Phase:** 1 (requirements) · **Feeds:** Phase 2 development plan

> **Revision 2 supersedes revision 1's experience findings entirely.** Revision 1 reported
> that Zangband inflated monster experience by a median of 20×. That figure was an artifact
> of comparing `mexp` fields across versions that define `mexp` differently. Tracing the
> award formula (§2.5) shows Zangband's actual experience per kill is **0.98× vanilla's** —
> statistically unchanged. Any conclusion drawn from revision 1's §2.5 curve is void.

Companion to the three feature-family requirement docs (world & towns, player systems,
content & flavour). Those say *what to build*; this one says *what numbers it gets*.

Scope: how gameplay quantities from Zangband 2.7.5-pre1 are carried onto Angband 4.2.6.
Covers the monster bestiary, which has been measured. Objects, artifacts and ego items are
in scope but **not yet measured** — see [Unmeasured](#5-unmeasured).

---

## 1. Why this document exists

Zangband's data files and Angband 4.2's data files both descend from Angband 2.8.1, but
they drifted independently for roughly twenty-five years. A field-for-field translation of
Zangband's data into 4.2's format would import that drift silently and produce a game that
is neither Zangband's balance nor 4.2's.

Two distinct kinds of difference are tangled together in the files, and the whole approach
turns on separating them:

- **Scale and semantic differences** — the same intent expressed in different units, or
  read by a different formula. These must be *converted*. Failing to convert them produces
  silent, unintended balance shifts with nothing in the data to hint at it.
- **Deliberate design differences** — Zangband genuinely chose different numbers. These are
  the "spirit" and must be *decided*, once and globally, not inherited per-monster.

§2.5 is the cautionary example: the single largest apparent difference in the entire data
set turned out to be a units change worth nothing at all. **No data-file figure in this
document may be trusted until the code that reads it has been checked.**

---

## 2. Measured evidence

Sources: [archive/angband-281/lib/edit/r_info.txt](../../archive/angband-281/lib/edit/r_info.txt),
[archive/zangband/lib/edit/r_info.txt](../../archive/zangband/lib/edit/r_info.txt),
[lib/gamedata/monster.txt](../../lib/gamedata/monster.txt).

Corpus: 537 monsters in 2.8.1, 884 in Zangband, 624 in 4.2.6.
450 shared between 2.8.1 and Zangband; 434 shared between 2.8.1 and 4.2.6.

### 2.1 The player experience table never changed

`player_exp[PY_MAX_LEVEL]` is **byte-identical across all three versions** — 50 entries,
10 → 5,000,000. Verified at
[archive/angband-281/src/tables.c:1264](../../archive/angband-281/src/tables.c#L1264),
[archive/zangband/src/tables.c:1664](../../archive/zangband/src/tables.c#L1664), and
[src/player.c:48](../../src/player.c#L48).

### 2.2 Angband 4.2 barely moved monster statistics

| Metric (4.2.6 ÷ 2.8.1, shared monsters) | n | median | mean | p10 | p90 |
|---|---|---|---|---|---|
| `mexp` field | 426 | 1.00 | 1.23 | 1.00 | 1.71 |
| hit points | 434 | 1.03 | 1.58 | 1.00 | 2.64 |
| armour class (raw) | 434 | 1.20 | 1.29 | 1.00 | 1.50 |

4.2 uses the same award formula as 2.8.1 (§2.5), so the `mexp` comparison is valid here and
means what it appears to mean: vanilla's experience values are essentially untouched since
2.8.1.

### 2.3 Armour class is on a different scale, and the factor is not obvious

The to-hit roll divisors differ between versions:

| Version | Roll is against | Source |
|---|---|---|
| Angband 2.8.1 | `ac * 3 / 4` | [cmd1.c:38](../../archive/angband-281/src/cmd1.c#L38) |
| Zangband 2.7.5 | `ac * 3 / 4` (unchanged) | [melee1.c:78](../../archive/zangband/src/melee1.c#L78) |
| Angband 4.2.6 | `ac * 2 / 3` | [player-attack.c:207](../../src/player-attack.c#L207) |

So the scale-neutral conversion for raw AC values is **× 9/8 = 1.125**, *not* the 1.5
suggested by reading 4.2's `armor-class` comment in isolation.

With that adjustment, 4.2's monsters are only ~7% harder to hit than 2.8.1's on median
(effective-AC ratio 1.067, n=434). Individual monsters did move: the cave orc went 32 → 48
raw where scale-neutral would have been 36, making it ~33% harder to hit.

### 2.4 Zangband's monsters are markedly squishier

| Metric (Zangband ÷ 2.8.1, shared monsters) | n | median | mean | p10 | p90 |
|---|---|---|---|---|---|
| hit points | 450 | **0.73** | 0.77 | 0.63 | 1.00 |
| armour class (raw) | 450 | **0.50** | 0.65 | 0.43 | 1.00 |
| speed (delta) | 450 | **+0.0** | −0.0 | — | — |
| sleep (delta) | 450 | **+0.0** | −0.1 | — | — |

Zangband left speed and sleep alone entirely. It cut hit points by roughly a quarter and
**halved** armour class. Since the to-hit formula is unchanged from 2.8.1 (§2.3), the AC
halving is a true halving of effective armour, not a rescale.

### 2.5 Experience: the apparent 20× is a units change, not a balance change

The award formulas differ:

| Version | Experience awarded per kill | Source |
|---|---|---|
| Angband 2.8.1 | `mexp * r_ptr->level / p_ptr->lev` | [xtra2.c:2106](../../archive/angband-281/src/xtra2.c#L2106) |
| Zangband 2.7.5 | `mexp / p_ptr->lev` | [xtra2.c:813](../../archive/zangband/src/xtra2.c#L813) (`exp_for_kill`) |
| Angband 4.2.6 | `mexp * race->level / p->lev` | [mon-util.c:1072](../../src/mon-util.c#L1072) |

**Zangband dropped the monster-level term and folded it into the `mexp` value itself.** In
vanilla, `mexp` means *experience per monster level*; in Zangband it means *total
experience*. The two fields are not comparable, and the ~20× median ratio between them is
almost exactly the average monster level — which is precisely what the arithmetic predicts.

Comparing what the player actually receives — Zangband's `mexp` against 2.8.1's
`mexp × level` — gives the true picture:

| Zangband ÷ 2.8.1, experience per kill | n | median | mean | p10 | p25 | p75 | p90 |
|---|---|---|---|---|---|---|---|
| all shared monsters | 450 | **0.982** | 1.321 | 0.455 | 0.678 | 1.471 | 2.372 |

| Native depth | n | median |
|---|---|---|
| 0–9 | 108 | 1.063 |
| 10–19 | 93 | 0.873 |
| 20–29 | 68 | 1.023 |
| 30–39 | 93 | 0.980 |
| 40–49 | 35 | 0.781 |
| 50–59 | 25 | 0.848 |
| 60–69 | 6 | 0.898 |
| 70–79 | 12 | 1.359 |
| 80–89 | 5 | 2.417 |
| 90–99 | 4 | 2.451 |
| 100+ | 1 | 2.067 |

**Zangband's experience per kill is statistically indistinguishable from vanilla 2.8.1's
below depth 70.** Individual monsters were retuned substantially in both directions
(Jackal 13×, Floating eye 11×, Giant red ant 0.02×), but there is no systematic
acceleration. The apparent inflation above depth 70 rests on 22 monsters in total and is
not a sound basis for anything.

*(11 depth-0 monsters are excluded: 2.8.1 awards zero experience for them by construction,
so no ratio exists.)*

### 2.6 Nor do the thresholds or race factors accelerate levelling

`player_exp[]` is identical (§2.1), so thresholds are unchanged. The remaining lever is
`expfact`, applied to thresholds — not to gains — as
`player_exp[lev-1] * expfact / 100` in both
[Zangband files.c:2461](../../archive/zangband/src/files.c#L2461) and
[src/player.c:230](../../src/player.c#L230), and composed identically as
`rp_ptr->r_exp + cp_ptr->c_exp` ([birth.c:254](../../archive/zangband/src/birth.c#L254)).

For the ten races common to both versions, Zangband's values are equal or slightly
**higher** — Gnome 125→135, Dwarf 120→125, Half-Troll 120→137 — meaning marginally
*slower* levelling. Across all races the mean rises from 130 (2.8.1, 10 races) to 148
(Zangband, 31 races), the increase driven by expensive newcomers: Half-Titan 255,
Draconian 250, Amberite 225.

**Conclusion: Zangband did not implement faster levelling by any mechanism in the
experience system.**

### 2.7 What does make Zangband move faster

Pace comes from §2.4, not from the experience system. Monsters with 27% fewer hit points
and half the effective armour die considerably faster. At the same experience per kill,
more kills per unit of play time is still faster levelling in wall-clock terms — and it
also produces Zangband's characteristically higher-turnover, more lethal encounters.

### 2.8 Nightmare mode — a whole difficulty system we had missed

Found in Zangband's official documentation (DEC-16), not in the data files, and absent from
every earlier revision of this document. Source:
[nightmare.txt](https://web.archive.org/web/20220420164309/http://www.zangband.org/spoilers/nightmare.txt).

An **irreversible birth option**, described by its own documentation as *"designed to be
unfair and not intended to be winnable"*, carrying a significant score multiplier. What it
changes:

- **Monsters:** hit points **doubled**, speed **+5 to +10**, all start awake and can never
  be slept, scared or confused (nor stunned, except by Monks); a random chance to resist
  attacks they have no resistance to; faster hit point regeneration; `MULTIPLY` monsters
  breed faster with the cap raised **100 → 255**; more out-of-depth and group generation;
  corpses in line of sight may **resurrect** each turn.
- **Pets:** over time pets become merely friendly, and friendly monsters turn hostile.
- **Objects:** fewer per level, less out-of-depth; no reduced-energy device activation;
  positive timed effects have a 10% chance per turn of halving; +5% device failure;
  reflection fails twice as often; doubled artifact recharge; unreliable pseudo-ID;
  disenchantment resistance weakened; double resistance no longer protects inventory;
  scrolls may fail outright.
- **Player races:** Golems lose stun immunity, Spectres lose nether immunity, Vampires can
  never resist light.
- **AI options** are forced on regardless of player preference.

Two interactions with decisions already taken:

1. **It doubles monster hit points, against BAL-13's ×0.73.** The two multiply to ×1.46, so
   nightmare mode must compose with the lethality scalar rather than replace it.
2. **The pet-decay behaviour presumes the allegiance model** of PLR-22, which
   [phase1-player-systems.md](phase1-player-systems.md) Q3 proposes deferring. Nightmare
   mode cannot be complete before pets are.

### 2.9 New content

387 monsters exist in Zangband but in neither 2.8.1 nor 4.2.6 — the actual content to port.
(Measured at 389 until the identity pass behind CNT-11 found two that Zangband had renamed
rather than added; see `tools/zconv/renames.toml`.)
By depth band: 0–9:29, 10–19:44, 20–29:36, 30–39:39, 40–49:34, 50–59:60, 60–69:50,
70–79:32, 80–89:21, 90–99:39, 100+:5.

---

## 3. Decisions recorded

**D-1 — The Zangband experience system is adopted as-is; no house multiplier.** Revision
1's "faster levelling" decision rested on figures that §2.5 and §2.6 have since voided.
Confirmed by project owner: the project follows Zangband's actual system rather than
compensating for it. Experience per kill, level thresholds and `expfact` all stay at their
measured values — there is no acceleration to reproduce, and none is invented. Progression
pace comes instead from §2.7: Zangband's substantially squishier monsters, which raise
kills per unit of play time at unchanged experience per kill.

**D-2 — Shared monsters keep 4.2's balance, not Zangband's.** Monsters present in both
Zangband and 4.2.6 are common inheritance from 2.8.1 and are not what makes Zangband
distinctive. Overwriting 4.2's tuning for them would discard twenty-five years of
balancing to no thematic benefit.

**D-4 — Zangband's lethality is applied as a global scalar on 4.2's values.** D-1 and D-2
as originally written cancelled out: D-1 declines any experience multiplier and rests all
pacing on §2.7's squishier monsters, while D-2 keeps 4.2's hit points and armour class for
most of the bestiary — leaving neither lever engaged and the game at vanilla 4.2 pace with
Zangband decoration. Resolved by scaling 4.2's own values by Zangband's measured deltas
(§2.4) rather than importing Zangband's per-monster numbers. This preserves 4.2's *relative*
tuning between monsters while adopting Zangband's *absolute* lethality. Confirmed by project
owner. Implemented by BAL-13.

**D-3 — Zangband-only content is calibrated onto 4.2's curve.** The 387 monsters in §2.9
have no 4.2 counterpart to inherit from. Their absolute numbers are derived from 4.2's own
curve; Zangband's numbers are used only as a *relative* signal of role.

---

## 4. Requirements

### Experience

**BAL-01 — The player experience table is not modified.** `player_exp[]` in
[src/player.c:48](../../src/player.c#L48) stays as-is. Rationale: §2.1 — all three
ancestors agree on it, so it carries no Zangband signal.

**BAL-02 — Zangband `mexp` values are divided by the monster's level on import.** Zangband
stores total experience; 4.2 expects experience per monster level (§2.5). Importing a
Zangband `mexp` without this division inflates the award by a factor equal to the
monster's depth — roughly 20× on average, and worst at exactly the depths where it does
most damage. Applies to BAL-08 monsters.

**BAL-03 — No global experience multiplier is applied.** §2.5 and §2.6 establish there is
none to reproduce, and D-1 declines to invent one. Should a house multiplier ever be
introduced it must be recorded as such under D-1 and never presented as Zangband behaviour.

**BAL-04 — Race and class `expfact` values follow 4.2's conventions for 4.2's races.**
Zangband's values are imported only for races imported alongside them (Amberite 225,
Draconian 250, Half-Titan 255 and similar), where they are part of that race's design.
Rationale: §2.6 — for shared races the two versions barely differ, so there is nothing to
gain by importing.

### Scale conversion

**BAL-05 — Raw armour class values imported from Zangband are multiplied by 9/8.**
Compensates for the `3/4` → `2/3` divisor change in §2.3. Applies only to values sourced
from Zangband; 4.2's own values are already correct.

**BAL-06 — Hit points are converted from dice notation to 4.2's average-hp integer** as
`ndm → n(m+1)/2`, rounded.

**BAL-07 — `sleep` is mapped to 4.2's `sleepiness` semantics, not copied.** ✅ **Met**,
and the recovery method is recorded as DEC-40. 4.2 documents
sleepiness on a 0–255 scale at
[lib/gamedata/monster.txt:108](../../lib/gamedata/monster.txt#L108). §2.4 shows Zangband
never changed `sleep` from 2.8.1, so the correct mapping is whatever 2.8.1 → 4.2 mapping
vanilla itself used; this is recoverable from the 434 shared monsters.

**BAL-08 — Every imported numeric field must have its consuming formula checked in both
codebases before conversion.** §2.5 is the precedent: a field comparison without a formula
check produced a conclusion that was wrong by a factor of twenty. No exceptions.

### New content

**BAL-09 — Zangband-only monsters are given stats derived from 4.2's curve at their
intended depth.** Method: regress 4.2's `hit-points`, `armor-class` and `experience`
against `depth` over 4.2's own bestiary, place the imported monster at its Zangband depth,
and set values from the fitted curve. Zangband's own numbers set the monster's *offset*
from that curve (tanky / glassy / fast), not its absolute values. Implements D-3.

**BAL-10 — Zangband monster flags with no 4.2 equivalent are reported, never silently
dropped.** 4.2 restructured monsters around `monster_base.txt` templates with separate
`blow_methods.txt` and `blow_effects.txt`; Zangband's flat flag lists do not map
one-to-one.

### Lethality

**BAL-13 — A global lethality scalar is applied to every monster at load time:
hit points × 0.73, armour class × 0.50.** Constants are Zangband's measured deltas from
§2.4. Applied on top of 4.2's values, after all other processing, to 4.2's and imported
monsters alike. Implements D-4.

**BAL-14 — The two scalars are named constants exposed in `constants.txt`, not literals.**
Rationale: they are the project's primary balance dial and will be tuned during playtest.
Keeping them in data means retuning requires no rebuild, and BAL-09's curve fitting must
read them rather than assume 1.0.

### Nightmare mode

**BAL-15 — Nightmare mode is an irreversible birth option** applying the modifier set in
§2.8, with an associated score multiplier.

**BAL-16 — Nightmare modifiers compose with BAL-13's lethality scalar, they do not replace
it.** Monster hit points under nightmare are `base × 0.73 × 2`. Rationale: §2.8 — treating
the doubling as an override would silently discard the project's primary balance dial.

**BAL-17 — Nightmare mode is implemented last within each feature family**, after that
family's normal behaviour is playtested. Rationale: it is a modifier on systems that must
first exist and be balanced; and its pet-decay behaviour is blocked on PLR-22 regardless.

### Tooling

**BAL-11 — The conversion tool's primary output is a review report, not data files.** For
every entry it must emit: the values chosen, which requirement produced them, a confidence
level, every unmapped flag, and an explicit list of every number it had to invent.

**BAL-12 — Conversion is re-runnable and deterministic.** Hand-edits to generated data are
recorded as overrides in a separate file, never applied to generated output directly, so a
re-run does not silently discard tuning work.

---

## 5. Unmeasured

In scope for balance calibration, not yet analysed. Each needs the §2 treatment — and per
BAL-08, each needs its consuming formula checked, not just its data compared:

- **Objects** — `k_info.txt` → [lib/gamedata/object.txt](../../lib/gamedata/object.txt)
- **Artifacts** — `a_info.txt` → [lib/gamedata/artifact.txt](../../lib/gamedata/artifact.txt)
- **Ego items** — `e_info.txt` → [lib/gamedata/ego_item.txt](../../lib/gamedata/ego_item.txt)
- **Player-side power** — 4.2 reworked classes and spells substantially and added classes
  Zangband never had. Monster difficulty is only meaningful relative to player capability,
  so the §2 monster figures are necessary but not sufficient.

Expect the object side to be harder: 4.2 introduced runes and a curse-as-object system with
no Zangband analogue.

---

## Open questions

1. ~~**How is the 2.8.1 → 4.2 `sleepiness` mapping recovered?**~~ **Settled by DEC-40:
   derived, not assumed.** `derive_sleep_mapping()` observes every pair across the 434
   shared monsters. 21 distinct source values, six unambiguous and fifteen resolved by
   median — and for 13 of the 21 the recovered value *is* the source value, because 4.2
   never rescaled sleepiness, it edited individual monsters. BAL-07 is met.
2. ~~Do the 389 imported monsters all belong?~~ **Settled by DEC-19: theme first.** Import
   everything carrying Amber, Mythos or Chaos identity; take the generic tail only where it
   fills a gap. Reduces the volume of BAL-09 calibration work proportionately.
3. **Are the §2.4 deltas the right constants, or merely the measured ones?** BAL-13 adopts
   0.73 and 0.50 because that is what Zangband did relative to 2.8.1. But 4.2's monsters
   are not 2.8.1's — §2.2 shows hit points drifted upward (mean 1.58×, p90 2.64×) even
   though the median barely moved. Applying Zangband's ratio to a distribution that has
   itself shifted may overshoot for the monsters vanilla beefed up. Resolvable only by
   playtest; BAL-14 exists so that retuning is cheap when it is.
