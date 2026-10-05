# Eight decisions waiting on the project owner

Written 26 September 2026, at 3.124.18. Each one is a choice, not a defect: the
code is coherent either way. Where the answer looks obviously "leave it", that
is said so it can be taken in one line.

---

## 1. ~~The Sprite's sleeping dust is stronger than the archive's~~ — DECIDED

**Ruled 27 September 2026: restore the two forms, keep the doubled power.**
Built in 3.124.20, recorded as DEC-106. Left here for the reasoning; the
original text follows.

### The original entry

**The choice.** Whether to keep our version or restore Zangband's two bands.

**Archive.** `racial.c:575`: below level 25 it is `sleep_monsters_touch()` — a
radius-1 ball centred on the Sprite. At 25 and above it is `sleep_monsters()`,
everything in line of sight. Power is `plev` in both.

**Ours.** `PROJECT_LOS:SLEEP_ALL` from level 12, power `PLAYER_LEVEL * 2`. So
stronger twice over: the line-of-sight version thirteen levels early, at double
the saving-throw power.

**Cost.** Data only, in `p_race.txt`: a `power-when` band pair and one
expression. Under an hour with a test.

**Recommendation: restore the archive.** A mass sleep in line of sight at level
12 is one of the strongest early-game buttons in the game, and the archive
plainly gated it on purpose. There is no record of the change being a design
choice — it reads as an import shortcut, and DEC-20 makes the archive
authoritative where nobody decided otherwise.

---

## 2. ~~The Vampire's bite: range, damage, and the food it does not give~~ — PART DECIDED

**Ruled 27 September 2026: add the nutrition; range and damage stay.** The
nutrition is built in 3.124.22, recorded as DEC-108. The range and the damage
curve remain as ours, deliberately and by the same ruling. Original entry
follows.

### The original entry

**The choice.** Three separable divergences. They can be taken apart.

**Archive.** `racial.c:528`: a directional bite on an *adjacent* monster, damage
`plev + randint1(plev) * MAX(1, plev / 10)`, and on success it feeds the
Vampire `100 x damage` food points, capped below Full.

**Ours.** A `BOLT:NETHER` at range for `PLAYER_LEVEL`, plus `HEAL_HP` for
`PLAYER_LEVEL / 2`. No nutrition at all.

**Cost.** The food is data — a `NOURISH` effect on the power, an hour. The range
is code: there is no adjacent-only targeting mode for a racial power, so it
would need one. The damage curve is data.

**Recommendation: add the nutrition, leave the range and the damage.** The food
is the only part with a play consequence: `BLOOD_DIET` gives the Vampire the
archive's penalty — ordinary food at a tenth — without the archive's remedy, and
the race is left eating Remove Hunger scrolls. That is a hole, not a balance
choice. The range and the damage curve are balance decisions somebody already
made, nothing has gone wrong because of them, and changing them now is a
rebalance in search of a complaint.

---

## 3. ~~The Skeleton's potions smashing on the drinker~~ — DECIDED

**Ruled 2 October 2026, both halves. Potions work normally -- "it is magic" --
recorded as DEC-119. The food stays as ours, the Skeleton keeping the
twentieth the archive denies it, recorded as DEC-121. No code change was
needed for either: this tree never implemented the potion smash, and the
twentieth is what it already does. DEC-122 closes the related Ghoul question
the same way.** Original entry follows.

### The original entry

**The choice.** Whether to import it at all.

**Archive.** `cmd6.c:200`: a Skeleton that quaffs a potion gets
`potion_smash_effect()` at its own grid — the potion's shatter effect, on
itself — and gains no nutrition from it. Its food behaves similarly: anything
that is not waybread or biscuit "falls through your jaws" and lands on the floor
intact, rather than being eaten at a reduced rate.

**Ours.** `CANT_EAT`, which is a twentieth of the nutrition. Potions are normal.

**Cost.** Code: a hook in the quaff path plus the food-falls-through behaviour
in the eat path. Half a day, and it touches object handling.

**Recommendation: no change.** This is a trap with no signposting — a new
Skeleton quaffs Cure Light Wounds and takes a shatter effect the game never
warned them about. It was flavour in 1998 and it would read as a bug today.
If you want it, it needs a message, a manual entry and a reason, which makes it
a feature to schedule rather than a fidelity gap to close. The food-falls-through
half is more defensible than the potion half if you want only one.

---

## 4. ~~Seventeen races share the Human backstory~~ — DECIDED AND BUILT

**Ruled and shipped 1-2 October 2026.** The voice was ruled as comic social
satire in Angband's own register; the Imp was ruled summoned and the Vampire
ruled to keep parentage. Built in `287c54b46` as DEC-118: 57 charts and 187
phrases, `player/backstory` holding the chains together, and five
concatenation defects found afterwards by reading the output and fixed in
`e0156c542`. No manual change was needed -- `docs/birth.rst:25` already said
background history is determined by race, which this made true. Original entry
follows.

### The original entry

**The choice.** Whether to write seventeen backstories, and who writes them.

**Archive.** Nothing to port. Zangband has no per-race history tables — I
checked. This is authorship, not transcription.

**Ours.** Every imported race runs the Human history chain, so a Spectre and a
Golem are both told they are the illegitimate child of a landed knight.

**Cost.** Data in `history.txt`, roughly four entries per race, so around
seventy short paragraphs. No engineering at all. It is a writing job.

**Recommendation: do it, and you write it — but not yet.** It is the largest
player-visible gap left in the project and it is pure content, so it does not
block anything and it does not risk anything. I would not have me invent
seventeen races' worth of lore: the tone of this game's prose is yours, and
generated backstory would read like generated backstory. Schedule it as a
content pass near a release.

---

## 5. ~~M11 stage 2 — the §2.8.5 menu~~ — ALL FOUR DECIDED, M11 CLOSED

**Ruled 3-4 October 2026: take all three of the ones that could be ruled on.**
`repro-max` 100 to 255 (DEC-126), the player's reflection failing twice as
often (DEC-127), and shopkeepers who never rotate and carry the smallest
purses (DEC-128). All three are ours under BAL-18 rather than ports, all three
are gated on the birth option, and the manual describes them in a section of
their own kept separate from the sixteen it ports.

**Nastier mutations was the last of the four and is now settled too.** It was
scoped first, because it could not be ruled on as written, then ruled and
built as DEC-129: the ten continuous mutations that are pure penalty weighted
double, measured at 0.1213 to 0.2181. **With that, M11's exit criterion is
met and the milestone is closed** -- recorded in
`phase2-development-plan.md`.

**This was what M11's exit criterion waited on, and it no longer does.** The
criterion is *"stage 1 complete and every stage-2 addition recorded as a
decision"*, and DEC-84 established that declining counts as recording. All
four are recorded.

Original entry follows, and its recommendations are superseded: the
shopkeepers it said to leave were taken, on the grounds that "players notice
and complain" is a weak objection to an opt-in mode advertised as brutal.

### The original entry

**The choice.** DEC-84 records that nothing in §2.8.5 has been ruled either way.
Four items are flagged cheap and clearly good. A view on each:

- **`repro-max` 100 → 255.** *Take it.* One constant. It only matters on a
  level overrun by breeders, which is the situation the cap exists for.
- **Reflection's one-in-ten doubled.** *Take it.* One number, and it makes a
  minor property worth having.
- **Store owners who never rotate and carry the smallest purses.** *Leave it.*
  This makes shops worse in a way players notice and complain about, and the
  argument for it is fidelity rather than play.
- **Nastier mutations.** *Needs scoping before it can be decided.* "Nastier" is
  not a change until somebody says which mutations and how much; I would not
  take this as written.

**Cost.** The first two are one line each. The third is small. The fourth is
unbounded until scoped.

**Recommendation: take the first two now, decline the third, send the fourth
back for scoping.** That closes M11's exit criterion, which needs every stage-2
addition *recorded as a decision* — declining counts.

---

## 6. ~~Open question 5 — `PASS_WALL` in the random-power pool~~ — DECIDED

**Ruled 28 September 2026: take it out of the pool.** Built in 3.124.23,
recorded as DEC-109. Original entry follows.

### The original entry

**The choice.** Whether a randomly generated ego may grant permanent wall-walking.

**Background.** `OF_PASS_WALL` lives in `OFT_MISC`, which is the pool
`KF_RAND_POWER` draws from. So an ordinary ego item can roll it. DEC-85 confirms
the interaction is *coherent* — a wraith wearing one does not die when the form
ends — but coherent is not the same as intended.

**Cost.** One line: take the flag out of that pool.

**Recommendation: take it out.** Permanent pass-wall is a bigger power than
anything else in `OFT_MISC` by a wide margin — it removes the dungeon as an
obstacle — and it is not a Zangband ego property. It is there by accident, as a
new flag landing in an old grouping. Nothing has been built on it and no test
depends on it.

---

## 7. ~~The Draconian's Druid, Necromancer and Blackguard breath~~ — DECIDED

**Ruled 28 September 2026: leave it.** Recorded as DEC-110. Nothing built. Original entry follows.

### The original entry

**The choice.** Whether to leave three classes on the fallback or give them
entries.

**Archive.** Zangband has eleven classes and its switch covers all eleven, so it
never needed a default. The Druid, the Necromancer and the Blackguard are 4.2's
and have no archive answer.

**Ours.** They fall through to fire and cold — which is what the archive's code
would do with an unknown class. Documented in `p_race.txt` and DEC-87.

**Cost.** Data: three `power-when-class` bands. An hour.

**Recommendation: leave it.** The fallback is the archive's own behaviour for a
class it does not know, it is documented, and fire-and-cold is a reasonable
breath for all three. Inventing three bespoke element sets would be us designing
rather than importing, and there is no complaint attached. Revisit if somebody
plays a Draconian Necromancer and finds it dull.

---

## 8. ~~The road-to-gate drift~~ — DECIDED

**Ruled 28 September 2026: leave it.** Recorded as DEC-111. Nothing built. Original entry follows.

### The original entry

**The choice.** Whether ten grids is close enough.

**Now.** DEC-101 fixed roads arriving at an ungated wall. What remains is drift:
the gate is cut at the road's block centre and then walked along the wall past
anything with a shop behind it. Measured over eighteen worlds and 426 arrivals:
**355 land on the gate exactly, 406 are within one grid, and the two worst are 8
and 10** — both on a 132x34 great city where the road meets the wall near a
corner. Before DEC-101 the routine case was twelve to twenty-six.

**Cost.** Code in the town generator: the cutter would have to move a shop or
refuse to place one behind a road's column. That is a change to town layout for
one road in 426.

**Recommendation: no change.** The number that mattered — roads arriving at a
blank wall — is zero. Ten grids on the largest town, once in four hundred, is
a short walk along a wall, not a dead end, and the fix risks the thing that is
currently working.

---

## 9. ~~Uniques and our timed-effect saving throw~~ — DECIDED

**Ruled 27 September 2026: restore the archive's flat immunity, all four
effects.** Built in 3.124.21, recorded as DEC-107. Original entry follows.

### The original entry

**The choice.** Whether a unique monster can be slept, slowed, confused or
frightened at all. This came out of the Sprite decision (DEC-106) and is wider
than that race: it is the model, not one power.

**Archive.** Flat immunity, checked before anything else. `spells1.c` gates
`GF_OLD_SLEEP`, `GF_OLD_SLOW`, `GF_OLD_CONF` and `GF_TURN_ALL` on
`FLAG(r_ptr, RF_UNIQUE)` first, so a unique is never slept, slowed, confused or
made to flee, by anything, at any character level.

**Ours.** 4.2's graded model. `mon-timed.c:74` gives a unique a *second* saving
throw and resists if either succeeds — `resist_chance = MIN(90, race->level +
MAX(0, 25 - timer/2))`, so the effective resist is `1 - (1-p)²`. Concretely, a
level-20 unique against a level-25 Sprite's dust: `p = 0.20`, effective resist
36%, so **it is slept about two tries in three**. At the 90 cap — a race level
of 90 or above, which is Morgoth's neighbourhood — it is 99% and the exposure
is negligible. So the gap is at *low and mid-level uniques*, which is where a
player meets most of them.

**Cost.** Small in code, wide in consequence. A `unique_immune` column in
`list-mon-timed.h` and a check in `does_resist()` is perhaps ten lines. What it
touches is every source of those four effects in the game — spells, wands,
staves, racial and class powers, monster spells — and the balance that has been
played and swept against them since the races landed.

**Recommendation: restore the archive's flat immunity, for the four effects the
archive covers.** The argument that carried the Sprite carries here and is
stronger: DEC-20 makes the archive authoritative where nobody decided otherwise,
and nobody decided this — it is 4.2's model surviving because the port never
asked the question. And the category matters. A unique is the fight the game
means you to have; sleeping one and walking round it, or sleeping it and taking
free swings, is not a tactic the design intends to sell for twelve mana.

**Two honest caveats.** First, 4.2's graded model is *deliberate* — upstream
replaced flat immunities with saving throws on purpose, and we are a port onto
4.2, so this is one of the places where the two parents genuinely disagree
rather than one of them being silent. That is a real argument for leaving it.
Second, if you want a narrower change: **sleep and hold** are the two that
remove a monster from the fight outright, while slow, confusion and fear only
degrade it. Restoring immunity for those two alone would take most of the value
for a fraction of the blast radius — though it is a third position belonging to
neither game, and would need recording as ours.

**Not built. Nothing has been changed for this.**

