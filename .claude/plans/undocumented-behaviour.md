# Implemented behaviour the manual never mentions

Written 29 September 2026, from the manual review of 28 September. That review also found
some ninety factual errors in `docs/*.rst`; those were fixed and landed, and are not repeated
here. **This file is the other half — behaviour that exists in the code, that a player would
notice, and that no manual chapter describes.** Each entry is a writing job, not a defect: the
code is right, the documentation is silent.

Ordered by how much a player would want to know, not by how much work each is. Band A first.

---

## How to read the provenance marks

The review was carried out partly by delegated readers working from the source. Each entry
below carries the file and line its claim rests on, but **only the entries marked
`[verified here]` were re-read directly**. The rest are marked `[read by review]` — a line
reference from a source read, not confirmed a second time. Two entries rest on the decision
log rather than on a code path and are marked `[decision log]`; those are the ones to check
first if something does not match.

- `[verified here]` — the file was opened and the code read, 28-29 September.
- `[read by review]` — cited from the source by the review, not re-read.
- `[decision log]` — sourced from `decisions.md`, **not** traced to a code path. Load-bearing
  only once confirmed.
- `[absence]` — the finding is that something is *missing* from `docs/`, established by grep.

**One caveat that applies throughout.** The tier 1-3 fixes landed after this list was drawn
up. A few entries below sit next to text that was rewritten in that pass, so some may have
been absorbed already. Grep the chapter before writing — particularly entries 9, 10, 17, 21
and 22, which are adjacent to corrections that landed.

---

## Where this stands — 4 October 2026

All twenty-five are now resolved. **Twenty written, four dropped, one (25) stale on arrival.**

Dropped, each with its reopening condition in the entry: **4** (savefile compatibility — the
decision log says the opposite of the claim), **22** (nightmare caps — real code, unreachable
at the current lethality scalar; and the arrival-energy half is backwards), **24** (the Imp's
affinity — it has one, and the manual already says so), and half of **13** (`RES_TELE` does
not block teleport-to; only the swap).

**What the exercise actually caught.** The dropped entries are not the headline. Four entries
turned out to sit *next to a false sentence that the tier passes had left standing* — a claim
corrected in one passage and restated, uncorrected, somewhere else in the same file:

| Entry | File | The sentence that was still there |
|---|---|---|
| 9 | `wilderness.rst:188` | "Monsters cannot swim, so deep water is a reliable way of breaking pursuit." |
| 10 | `birth.rst` | "Reaching past what is left in the pool makes it markedly worse" — the DEC-56 penalty. |
| 20 | `dungeon.rst` | "You must be in the dungeon for the store to restock." |
| 21 | `wilderness.rst:236` | "Towns are picked out in white." |

All four are fixed. The pattern is worth keeping: **a correction that rewrites one passage does
not find the same claim restated elsewhere in the file**, and the backlog's "check first" notes
pointed at exactly the places where this had happened. Checking first paid for itself four
times and cost nothing.

Line numbers in the entries below were indicative, as warned. Roughly a third had drifted —
`wild.c`'s sea constants by five, `ui-options.c` and `ui-map.c` by a few, `player-mutation.c`
by the eight that entry 25 records. Every citation used for a write was re-read; the corrected
reference is in the entry.

---

# Band A — a player changes what they do

## ~~1. The patron's cruelty shifts with your virtues~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/birth.rst`, the Patrons section. Verified first: `player-util.c:2515` reads Chance and Individualism up, Harmony and Temperance down.

**What.** The Chaos-Warrior's reward roll is not purely level-driven. `Chance` and
`Individualism` make the Lord of Chaos gentler; `Harmony` and `Temperance` make it harsher.
The four are summed and the roll shifts one step per forty points. A character who has lived
chaotically for a long time is meaningfully safer at the patron's hands.

**Where.** `src/player-util.c:2515-2517` — `favour = V_CHANCE + V_INDIVIDUALISM - V_HARMONY -
V_TEMPERANCE; nasty = MAX(1, nasty + favour / 40);`. The comment above it at `:2505-2514`
explains the intent in full and is worth quoting from. `[verified here]`

**Chapter.** `docs/birth.rst`, the Patrons section at 1103-1106, which currently presents the
odds as a function of character level alone. `docs/virtues.rst:75-76` should cross-reference it.

**Why it leads.** This is the single largest lever a Chaos-Warrior has over its own fate and
nothing in the game tells the player it exists. Note the ceiling: one virtue at the +/-125 cap
is three steps, all four at cap is twelve — the review's tier 3 found `virtues.rst` claiming
"two steps", so check whether that fix already introduced the fuller explanation.

---

## ~~2. A pet more than ten squares away abandons its fight and comes looking for you~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/pets.rst`, as a note below the leash table rather than inside it. Verified: `PET_SEEK_DIST` is 10 at `monster.h:422`, applied at `mon-move.c:1035`.

**What.** `PET_SEEK_DIST` is a hard override on *every* leash setting, including "Seek and
destroy". A pet whose distance from you exceeds ten has its follow distance clamped and breaks
off whatever it is doing to close the gap.

**Where.** `src/monster.h:422` (`#define PET_SEEK_DIST 10`), with an explanatory comment at
`:416` saying explicitly that it is "not one of the five a player can choose". Applied at
`src/mon-move.c:1035` (`bool distant = mon->cdis > PET_SEEK_DIST;`) and `:1049`.
`[verified here]`

**Chapter.** `docs/pets.rst:190-192`, which currently tells the player that seek-and-destroy
pets "go anywhere" and that you "lose track of them". That is the opposite of what the code
does.

**Why it matters.** It silently defeats the one leash setting a player picks in order to send
pets away, and the behaviour reads as a bug when you meet it without knowing the rule.

---

## ~~3. Sorcery's *Alchemy* spell is not mentioned anywhere in the manual, nor are its terms~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/realms.rst` Sorcery section, with the terms, and a cross-reference from `docs/mutations.rst` for the Midas touch, which shares them.

**What.** Two things, and the second is the bigger one.

First, the spell exists and `docs/realms.rst` never names it: six classes carry it -- Mage,
Priest, Rogue, Ranger, Warrior-Mage and High-Mage
(`lib/gamedata/class.txt:553, 2086, 3978, 4641, 7058, 8332`, all `effect:ALCHEMY`). A grep for
"alchemy" across `docs/realms.rst` returns **zero** hits. `[verified here]`

Second, the terms of the object-to-gold conversion are documented nowhere at all — not for the
spell and not for the Midas mutation, where `docs/mutations.rst:88-92` says only that it
"works". The terms are: a third of the object's *real* value; capped at 30,000 gold
(`ALCHEMY_MAX`, `src/effects.h:76`, applied at `src/effect-handler-general.c:4215`); artifacts
refuse; a worthless object is destroyed for nothing; and the division happens **before** the
quantity multiplies, so a stack of ten two-gold items pays zero. `[verified here]`

**Chapter.** `docs/realms.rst`, Sorcery section (the spell itself); `docs/mutations.rst:90`
(the terms, shared). DEC-52 has the rationale. Currently the only statement of any of this in
`docs/` is a changelog line at `releases.rst:3519`.

**Why it matters.** The stack-of-ten-pays-zero rule will cost a player real money before they
work it out, and there is no way to work it out except by losing the items.

---

## 4. ~~"Your character survives an upgrade" is no longer safe to promise~~ — DROPPED, NOT TRUE

**Verified 4 October 2026 and not written, because it does not hold up.** The
entry rested on the decision log alone and said so. Traced:

- **DEC-57 is mischaracterised.** It is "Every entitlement Zangband gives is
  carried over" -- it does not refuse to load a saved caster.
- **DEC-90 says the opposite of what is claimed here.** Its own words are that
  an old file loads *cleanly*, that "the blast radius is empty", and that every
  release so far is pre-release so old savefiles do not matter.
- **There is no version gate.** The entry asked whether one had since been
  added that refuses old files; `src/savefile.c` has none, so nothing has
  closed and nothing has broken.

The residue is DEC-90's silent-misinterpretation point, which DEC-90 itself
weighed and accepted. `docs/download.rst:328` is not currently false, so
writing a warning there would be documenting a problem the game does not have.
**Revisit if the project ever ships non-pre-release**, which is the condition
DEC-90 names. Original entry follows.

**What.** `docs/download.rst:328` promises that a character survives an upgrade. Three
decisions have made that conditional: DEC-50 invalidated every casting class's savefile;
DEC-57 refuses to load a saved caster with spells recorded; and DEC-90 records an object-flag
index shift on 9 September (`OF(PASS_WALL)` inserted mid-list) under which a pre-9-September
file **loads cleanly and silently means something else**. The demonstrated case in the log is a
Hammer of Returning loading without Returning and granting pass-wall instead.

**Where.** `[decision log]` — DEC-50, DEC-57, DEC-90 in `.claude/plans/decisions.md`. **Not
traced to a code path by this review.** DEC-90's own text says the promise "stops being true
the moment the game ships to players who keep characters". Before writing anything, confirm the
current savefile version gate in `src/savefile.c` and whether a version bump has since been
added that refuses the old files outright — if it has, this entry becomes a much shorter note
and drops several places.

**Chapter.** `docs/download.rst:328`, and a cross-reference from `docs/balance.rst:83-90`.

**Why it is this high despite being unverified.** Silent misinterpretation of a savefile is the
one failure a player cannot detect, diagnose, or recover from. If it is real it outranks almost
everything else here; if the version gate has closed it, say so and move on.

---

## ~~5. Three Trump summons do not summon what their names say~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/realms.rst`, Trump section. All three re-verified against `class.txt`: Cyberdemon gives `HI_DEMON`, Phantasmal Servant `UNDEAD`, Conjure Elemental `AINU`. The entry's "Also" about a count of twelve against fourteen did not reproduce -- no such count is in the chapter now.

**What.** *Trump Cyberdemon* is `effect:SUMMON_PET:HI_DEMON` — a greater demon, not a
Cyberdemon. *Phantasmal Servant* is `SUMMON_PET:UNDEAD`. *Conjure Elemental* is
`SUMMON_PET:AINU` — a Maia, not an earth elemental. Two of the three also lost Zangband's
caster-level boost and now summon from the depth the caster is standing on.

**Where.** `lib/gamedata/class.txt:1357, 1365, 1409` (Mage), repeated at `:2853, 2861, 2905`
(Priest) and in the other four Trump classes. DEC-64 has the rationale. `[read by review]`

**Chapter.** `docs/realms.rst:308-311`, which names the ladder "up through the hounds, spiders,
reptiles, dragons and undead to *Trump Cyberdemon*" — phrasing that actively implies each spell
does what its name says.

**Also.** While in this section: the review found the chapter's count of pet-summoning Trump
spells is twelve where the data has fourteen (Phantasmal Servant and Conjure Elemental are the
two not counted). That was a tier 2 finding and may already be fixed — check before rewriting.

---

## ~~6. A Necromancer casts 25 points worse on a lit square~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/realms.rst`, Casting section. Verified at `player-spell.c:683`.

**What.** A flat +25 to the failure chance whenever a character with `PF_UNLIGHT` stands on a
lit square.

**Where.** `src/player-spell.c:683-685`, comment "Necromancers are punished by being on lit
squares". `[verified here]`

**Chapter.** `docs/realms.rst:585`, which introduces the casting rules with "two rules apply to
every realm" — this class-specific third one belongs immediately after. The Necromancer blurb
at `docs/birth.rst:782` says only that the class "shrouds itself in darkness", which a player
will read as flavour.

**Why it matters.** Twenty-five points is the difference between a reliable spell and a coin
flip, and the player has no way to attribute the swing to the square they are standing on.

---

## ~~7. Spell failure floors at 5% and caps at 50%~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/realms.rst`, Casting section, including that stun is applied after the cap. Verified at `player-spell.c:677` and the lines below it.

**What.** A character without `PF_ZERO_FAIL` never gets below 5% failure however good they get.
Every character caps at 50% however bad the odds look — but stunning is applied *after* the cap,
so a stunned caster can exceed it (+15, or +25 past stun 50).

**Where.** `src/player-spell.c:677-679` (the 5% floor), `:693-694`
(`if (chance > 50) chance = 50;`), `:696-700` (stun applied afterwards). `[verified here]`

**Chapter.** `docs/realms.rst`, the casting section. The asymmetry is worth stating plainly
because the manual **does** document the 95% cap on racial powers, so a reader currently has
half the picture and no reason to suspect the other half differs.

**While you are in that function.** Fear is +20 to spell failure too
(`src/player-spell.c:690`), and the comment there notes that fear-removal spells are
deliberately given much lower base rates so the penalty cannot lock a frightened character out
of curing it. That is a nice detail and is documented nowhere.

---

## ~~8. There is no inn in the starting village, so the quest chapter's opening advice never applies at home~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/quests.rst`, immediately under the advice it contradicts. Verified: `wild.c:1182` gives the starting town `WILD_SERVICE_MAGETOWER` and nothing else. `towns.rst:207` was checked and does *not* already cover this -- its warning is about fallen towns.

**What.** `docs/quests.rst` tells the player to "walk into an inn that is hiring". The starting
village returns a service mask of magetower and nothing else, so a player following the
instruction where they begin will not find one.

**Where.** `src/wild.c:1177-1179` — `wild_town_services()` returns
`1u << WILD_SERVICE_MAGETOWER` for the starting town. `[read by review]`

**Chapter.** `docs/quests.rst`, first paragraph; and a line in `docs/towns.rst` where the
starting village's fixed four shops are described.

**Why it matters.** It is the first thing a new player tries after reading the chapter, and it
fails silently — there is no message explaining why the village has no inn.

---

# Band B — a player is surprised, but not misled into a decision

## ~~9. Flying and swimming monsters cross water; the sea has a danger floor~~ — MOSTLY ABSORBED; ONE PART WRITTEN

**Checked 4 October 2026.** Two of the three facts were already in the chapter.

- **The crossing rule: already written**, at `wilderness.rst:89-100`, and it points at the
  data file for the counts rather than restating them. The citation had drifted: the comment
  carrying 117/91 is `mon-move.c:191-202` and the test `:203-208`.
- **The sea's danger floor and density: already written**, at `wilderness.rst:179-186`. Both
  constants moved too — `WILD_SEA_DENSITY` is `wild.c:3871`, `WILD_SEA_DEPTH` is `:3878`,
  applied at `:3962` and `:3998`.
- **The 8-grid exclusion: WRITTEN 4 October 2026** — `docs/wilderness.rst`, end of "What
  lives there". Verified at `wild.c:3969`.

**And one thing this entry missed.** `wilderness.rst:188` still read *"Monsters cannot swim,
so deep water is a reliable way of breaking pursuit"* — stranded in a later section, ninety
lines below the paragraph that says the opposite, and flatly false. The tier 1 pass rewrote
the deep-water section and did not find the same claim restated elsewhere in the file.
**Removed.** Worth recording as a pattern, not a one-off.

Original entry follows.

**What.** Three related facts. 117 imported races have `RF_CAN_FLY` and 91 have `RF_CAN_SWIM`,
so deep water stops far less than it appears to. The open sea has a danger floor of 8 and a
fixed encounter density regardless of how lawful the coast you waded off was. And nothing at
all is generated within 8 grids of the player.

**Where.** `src/mon-move.c:205-208` (the crossing rule); `src/wild.c:3874`
(`WILD_SEA_DEPTH 8`), `:3866` (`WILD_SEA_DENSITY 24`), applied at `:3960` and `:3995`;
`src/wild.c:3969` (the 8-grid exclusion). `[read by review]`

**Chapter.** `docs/wilderness.rst`, the deep-water section.

**Check first.** The tier 1 pass corrected `wilderness.rst:182`, which claimed water reliably
broke pursuit. That fix may already have introduced the swimming and flying counts. The sea
danger floor and the 8-grid exclusion are separate and were almost certainly not covered.

---

## ~~10. Racial powers: the price is randomised, and fear makes them harder~~ — HALF ABSORBED, HALF WRITTEN

**WRITTEN 4 October 2026.** The randomised price was already in `birth.rst` ("somewhere
between half the listed cost and all of it"); it was *not* in `command.rst`, and now is.
Fear was in neither and is now in both. Verified: `player-util.c:2377` (price),
`:2201` (`if (p->timed[TMD_AFRAID]) chance += 20;`). The price is charged before the failure
roll, so a failed power still costs — `command.rst` already said so.

**The "check first" note was right and the check paid for itself.** `birth.rst` still read
*"Reaching past what is left in the pool makes it markedly worse"* — the mana-shortfall
penalty DEC-56 removed. The tier 1 fix rewrote the surrounding sentences and left the claim
standing. **Corrected**, with DEC-56 cited, since a reader who meets the behaviour will want
to know it is deliberate. Second false claim found adjacent to a backlog entry rather than by
it. Original entry follows.

**What.** Two things the powers section omits. The cost you pay is
`randint1(cost - cost/2) + cost/2` — somewhere between half and all of the listed figure, not
the listed figure. And being afraid adds 20 points to the failure chance.

**Where.** `src/player-util.c:2377` (the randomised price), `:2201` (the fear penalty).
`[read by review]`

**Chapter.** `docs/command.rst:295-296` states the cost as though it were fixed;
`docs/birth.rst:676-681` lists level, stat, mana and stun as the inputs to failure and omits
fear.

**Check first.** `birth.rst:677-678` was a tier 1 finding (it claimed a mana-shortfall penalty
that DEC-56 removed). That fix rewrote the surrounding sentences, so confirm what the section
now says before adding to it.

---

## ~~11. The Mindcrafter's psionics have three undocumented growth bands and a backfire~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/birth.rst`, the psionics section, after the two paragraphs
that stop short of these three. All three re-verified against `class.txt`: the resistance
ladder is `power-when:17/21/25/29/33` on *armour your mind* (acid, fire, cold, elec, pois);
*pulverise* is `BALL:SOUND:1` then `:2` at 21; *loose a telekinetic wave* is `SPOT:FORCE:4` at
three times level, then `:8` at four times, at 40.

**The backfire half was already written** and written well — `birth.rst` has it as a warning
block, including the five bands and that racial powers do not backfire. What was missing was
any mention of it on `command.rst`'s "Use a power", which is where a player meets the command;
a cross-reference was added there.

**What.** *Armour your mind* grows a resistance ladder at levels 17, 21, 25, 29 and 33 — acid,
fire, cold, electricity, poison. *Pulverise* goes from radius 1 to radius 2 at level 21. *Loose
a telekinetic wave* goes from radius 4 to radius 8 and from 3x to 4x damage at level 40.
Separately: a failed **class** power — which for a Mindcrafter means any psionic — additionally
rolls a backfire on roughly half the failure chance.

**Where.** `lib/gamedata/class.txt:6294-6313` (resistance ladder), `:6277-6284` (pulverise),
`:6403-6410` (telekinetic wave); `src/player-util.c:2394-2402` (`player_mind_backfires()`). The
backfire bands themselves are at `src/player-util.c:2312-2340` — stun 45%, confusion 30%, mana
storm 11%, visions 10%, amnesia 4% — and those *are* documented at `birth.rst:1044ff`.
`[read by review]`

**Chapter.** `docs/birth.rst`, the psionics section, which documents growth bands for six other
powers and simply stops before these three. `docs/command.rst:298-301` describes power cost and
failure generally and never mentions that the Mindcrafter's failures bite back.

---

## ~~12. All five undead races start just after midnight, not only the Vampire~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/birth.rst`, one shared paragraph under the Vampire where
the midnight start is introduced, and the food scrolls added to the Spectre and Ghoul entries.
Verified: `player-birth.c:1485` keys on `PF_UNDEAD`; the flag is on Vampire, Skeleton, Zombie,
Spectre and Ghoul and on nothing else (Golem is not undead).

**The "Also" undercounts.** Six races take Remove Hunger scrolls in place of rations, not
four and not two: Vampire, Golem, Skeleton, Zombie, Spectre and Ghoul (`equip-instead` at
`p_race.txt:863, 939, 1150, 1183, 1224, 1262`). The manual already had four of the six; the
two missing were Spectre and Ghoul, which is what was written. The Ghoul is the sharper case
— `CANT_EAT` without `SLOW_DIGEST`, so it is as badly placed as a Skeleton, and its entry
said nothing about food at all while calling it a corpse-eater.

**What.** The midnight start is driven by `PF_UNDEAD`, so Skeleton, Zombie, Spectre and Ghoul
get it too. The manual attributes it solely to the Vampire's sunlight problem, which leaves the
other four looking like a scheduling accident.

**Where.** `src/player-birth.c:1485`, with a note at `lib/gamedata/p_race.txt:1119`.
`[read by review]`

**Also in the same family.** Spectre and Ghoul trade their starting rations for Scrolls of
Remove Hunger just as Vampire, Golem, Skeleton and Zombie do (`p_race.txt:1223, 1261`); the
manual lists only the other four.

**Chapter.** `docs/birth.rst`, the undead race entries; one shared sentence would cover all
five.

---

## ~~13. `RES_TELE` blocks your own teleport-to, not just teleport-away~~ — HALF WRITTEN, HALF DROPPED

**Half of this does not reproduce.** The claim is that `RES_TELE` stops a monster being
"swapped with *or pulled to* you". Traced: there are four `RF_RES_TELE` sites in the tree
(`effect-handler-general.c:4310`, `mon-util.c:209/212/218`) and none of them is in
`effect_handler_TELEPORT_TO`. **A monster with `RES_TELE` can be pulled to you.** Dropped.
Reopen if a `RES_TELE` test is ever added to `TELEPORT_TO`.

**The swap half holds and is WRITTEN 4 October 2026** — `docs/monsters.rst`, under the list
that presented itself as exhaustive ("so there is no side door"). Verified at
`effect-handler-general.c:4310`, inside `effect_handler_SWAP_POS`. The only thing in the game
that casts `SWAP_POS` is the swap-position mutation (`mutation.txt:208-219`), and
`mutations.rst:81-83` **already documents the interaction correctly** — so the gap was only
ever in `monsters.rst`, which is a narrower job than the entry describes.

**What.** A monster with `RES_TELE` also refuses to be swapped with or pulled to you — "Your
teleportation is blocked!"

**Where.** `src/effect-handler-general.c:4309`. `[read by review]`

**Chapter.** `docs/monsters.rst:135-137`, which lists three routes `RES_TELE` affects and
presents the list as exhaustive.

---

## ~~14. Quest terms: the pay is a formula, there are eight slots, and the offers are not uniform~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/quests.rst` (all four, plus the Justice note) and
`docs/towns.rst` (pay and slot count, with a pointer to `quests.rst` for the rest). All four
re-verified; three citations had drifted.

- Pay: `ui-map.c:1711-1712`, `max_num * (race->level + 1) * 20`, floored at 20. Written as
  prose with a worked example rather than as a formula.
- Slots: `constants.txt:542` (`wild:quest-slots:8`), refusal at six sites in `ui-map.c`, not
  the two cited. The chapter now names the data file rather than only the number.
- Depth: bounty `max_depth + 4` at `ui-map.c:1769`; dungeon job `rand_range(min, max)` clamped
  to `max_depth + 6` and back up to the dungeon's own minimum, `:1582-1584`. The clamp runs
  both ways, which the entry did not say and which is the part a player feels.
- Offer roll: `ui-map.c:1746-1759`. `randint0(5)` descending over four cases, so one roll in
  five offers none of them, and the kinds low in the switch are reachable from more starting
  points than the kinds high in it. Written as an order of likelihood rather than a table.
- Justice: `mon-speech.c:182`, +5, and the comment confirms it is the virtue's only writer.

**What.** Four things, all in the same chapter.

- Pay is `max_num * (race level + 1) * 20`, minimum 20 gold (`src/ui-map.c:1707-1709`).
  `docs/towns.rst:261` says only "be paid".
- You may hold **eight** quests, with a refusal message "You've enough on your plate already"
  (`lib/gamedata/constants.txt:542`, `src/ui-map.c:1598, 1795`). The manual says "several".
- Depth scaling: bounties draw from `max_depth + 4`, dungeon jobs from `max_depth + 6`, clamped
  to the dungeon's own range (`src/ui-map.c:1583-1586, 1769`). `docs/quests.rst:32-33` says only
  "near your own depth".
- The offer roll is **not** uniform: `randint0(5)` descending, with `one_in_(3)` for the
  wilderness kill and bounty as the terminal fallback (`src/ui-map.c:1738-1759`), so bounties
  dominate the mix far more than "every kind falls back to a bounty" suggests.

Also: collecting a bounty writes +5 to the Justice virtue (`src/mon-speech.c:176-179`), and it
is the only writer to Justice in the game.

**Where.** As cited. `[read by review]`

**Chapter.** `docs/quests.rst` throughout; the pay formula and slot count also belong in
`docs/towns.rst:256-275`.

---

## ~~15. Virtues change the inn's dream odds~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/towns.rst`, immediately below the law table so the two
cannot be read apart. Verified at `player-util.c:2688-2702`: Enlightenment plus Knowledge over
twenty on the true chance, Unlife plus Chance over twenty on the dark. `virtues.rst:70-76`
already said the same thing from its side and the two now agree.

**What.** `docs/towns.rst:299-315` presents the dream table as a pure function of the town's
law. Enlightenment and Knowledge also raise the true-dream chance; Unlife and Chance raise the
dark-dream chance.

**Where.** `src/player-util.c:2688-2702`. `[read by review]`

**Chapter.** `docs/towns.rst:299-315`. `docs/virtues.rst` already names the dream as one of the
two virtue consumers, so the two pages need to agree.

---

## ~~16. Eleven commands have keys in the tables and no description anywhere~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/command.rst`, eleven entries in the existing per-command
format, each filed in the section its command belongs to rather than in a list of leftovers.
All eleven re-verified in `ui-game.c` and confirmed absent from `command.rst` first. Each was
read in its handler before being described, which changed three of them:

- `^o` shows the **most recent** message only (`ui-knowledge.c:3831-3835` prints `message_str(0)`);
  pressing it again shows the same line. "Previous" in the key table is misleading.
- `^g` always takes the gold and then takes only what inscriptions and options already mark as
  yours (`cmd-pickup.c:408-438`) — it is the walk-onto-a-pile pass run by hand, not a bulk `g`.
- `S` lists the **class and race properties**, class first (`player-properties.c:62-102`), not
  powers; it is where "holds its life" is explained in the game rather than in the manual.

**What.** These appear in the `docs/playing.rst` keyset tables — so they are discoverable — but
no chapter says what they do.

| Key(s) | Command | Source |
|---|---|---|
| `S` (both) | View abilities | `src/ui-game.c:177` |
| `U` / `X` | Use an item (generic) | `src/ui-game.c:135` |
| `'` (both) | Target closest monster | `src/ui-game.c:147` |
| `n` / `^v` | Repeat previous command | `src/ui-game.c:256` |
| `^l` / `@` | Center map | `src/ui-game.c:254` |
| `^g` (both) | Do autopickup | `src/ui-game.c:257` |
| `^o` (both) | Show previous message | `src/ui-game.c:220` |
| `;` (both) | Walk (with pickup) | `src/ui-game.c:250` |
| `"` (both) | Load a single pref line | `src/ui-game.c:246` |
| `^w` (both) | Toggle wizard mode | `src/ui-game.c:255` |
| `^z` | Borg commands (if built) | `src/ui-game.c:260` |

`[read by review]`

**Chapter.** `docs/command.rst`, in the existing per-command format.

---

## ~~17. Sidebar mode, and four options-menu entries~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/option.rst`, as a new "The options menu" section at the
top. Rather than describing the four undocumented entries and leaving the rest implicit, the
whole menu is now tabulated with its keys, which also gives the three "Left Over Information"
values the keys they were missing. Sidebar mode gets prose below it.

Verified: the menu is `ui-options.c:2092-2114` (the entry is at `:2104`, as cited);
`do_cmd_sidebar_mode` is `:1133-1163`; the three modes are `SIDEBAR_LEFT/TOP/NONE` at
`ui-term.h:248-250`, consumed at `ui-display.c:961` and `:965` and `:1513`, and saved with the
character (`save.c:335`, `load.c:677`). **None** keeps the bottom status line and loses the hit
point and mana readouts — worth checking, because the obvious guess is that it loses both.

`birth_nightmare` was indeed already fixed; the note can go.

**What.** `=` then `o` cycles the sidebar between Left, Top and None. It changes the entire
main-screen layout and no in-scope chapter mentions it. Four further menu entries are
undescribed: `=` `t` (save autoinscriptions to pref file), `=` `u` (save char-screen options),
`=` `p` (load a user pref file), and the menu keys for the three "Left Over Information" values
— whose *values* are documented but whose keys are not given.

**Where.** `src/ui-options.c:1133-1163` (sidebar), `:2104` (its menu entry). `[read by review]`

**Chapter.** `docs/option.rst`.

**Note.** `birth_nightmare` was in this same family — undocumented in `option.rst` despite
having a 160-line chapter of its own. That was tier 2 and should already be fixed; check.

---

# Band C — completeness, small surprises, and one open question

## ~~18. Mutation side-effects that are never stated where the mutation is described~~ — WRITTEN

**WRITTEN 4 October 2026** — all four, each beside the thing it is about rather than collected
in one place: `docs/mutations.rst` for the Chance virtue (under "How you get them"), the WRAITH
constitution cost (inside the wraith-form topic), the INVULN wisdom cost and virtue charge
(under "Random"), and the melee blows; `docs/virtues.rst` for who is doing the choosing.

All four verified: `player-mutation.c:174` (Chance +1 on any gain); `mutation.txt:818`
(`CON[-3]`) and `:907` (`WIS[-2]`); `player-virtue.c:346-358` — note that the charge is in
`virtue_note_timed()`, keyed on `TMD_INVULN` rising from zero, which is *why* the mutation is
charged identically to a deliberate potion and is the detail that makes `virtues.rst`'s wording
wrong; `player-mutation.c:501` (`if (*dead) return;` between blows).

- Gaining **any** mutation raises the Chance virtue by 1 (`src/player-mutation.c:174`). A
  Beastman triggers this every few levels and will drift up the Chance scale without knowing
  why.
- `WRAITH` carries a permanent `CON[-3]`; `INVULN` carries a permanent `WIS[-2]`
  (`lib/gamedata/mutation.txt`). Neither cost appears where the mutation is described.
- `INVULN` firing **unasked** costs 25 virtue points — -5 Temperance, Honour and Sacrifice, -10
  Valour (`src/player-virtue.c:353-357`). `docs/virtues.rst:41-42` presents that charge as
  something the player chooses to do.
- Melee mutation blows stop once the monster dies (`src/player-mutation.c:448-449`), so a
  character with five of them does not always land five.

`[read by review]`

**Chapter.** `docs/mutations.rst` for the first three; `docs/virtues.rst:41-42` needs the
correction about who is choosing.

---

## ~~19. Charming a monster moves two virtues, and the manual documents only the loss~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/pets.rst`, directly after the existing note about turning on
a pet, so the two directions sit together. Verified at `project-mon.c:1121-1124`.

**What.** Charming gives -1 Individualism always, and +1 Nature if the target is an animal.
`docs/pets.rst` documents only the virtue *loss* from turning on a pet.

**Where.** `src/project-mon.c:1121-1124`. `[read by review]`

**Chapter.** `docs/pets.rst`, alongside the existing virtue note.

---

## ~~20. Per-town store restocking is the rule a player actually notices~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/dungeon.rst`. Verified at `store.c:1492-1511`.

The entry undersells the problem: `dungeon.rst` did not merely omit the per-town rule, it
asserted *"You must be in the dungeon for the store to restock"*, which the per-town rule makes
false — walking to another town restocks without going underground. The false sentence is gone
and the two chapters now agree, with `dungeon.rst` pointing at `towns.rst` for the detail.

**What.** `docs/dungeon.rst:313-321` documents only the daily / 10,000-turn restock and says
"you must be in the dungeon for the store to restock". The rule a player meets is the per-town
one, keyed on `stock_town` and the store's quality tier.

**Where.** `src/store.c:1492-1511` (`store_enter()`). `[read by review]`

**Chapter.** `docs/dungeon.rst:313-321`. `docs/towns.rst:101-103` already documents it
correctly, so this is a matter of making the two agree.

---

## ~~21. The world map prints more than the manual describes~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/wilderness.rst`, the world-map section. Verified at
`ui-map.c:1100-1119` (coordinates, world size, legend) and `:1083-1085` (the margin colour).

**The "check first" was worth doing and came out the other way.** `wilderness.rst:236` still
read "Towns are picked out in white", which the band colours make false — towns are drawn in
one of four colours by band (`wild_town_band_attr`, `ui-map.c:1042-1044`), dungeons in light
red, and light white means a reserved margin and nothing else. The tier 2 fix did not reach it.
Corrected and the legend written out, naming the bands rather than the colour values.

**What.** World coordinates, the world's size, and a colour legend naming the four town bands
plus "dungeon". Blocks reserved as a town's or dungeon's margin are drawn in light white.

**Where.** `src/ui-map.c:1103-1119`, with the margin colour at `:1085`. `[read by review]`

**Chapter.** `docs/wilderness.rst`, the world-map section.

**Check first.** `wilderness.rst:230` ("towns are picked out in white") was a tier 2 fix, and
the corrected text may already describe the legend.

---

## 22. ~~Nightmare mode's multipliers have caps, and one base differs from Zangband's~~ — DROPPED, UNREACHABLE AND PART UNTRUE

**Verified 4 October 2026 and not written.** The two caps are real code and neither can be
reached; the third claim is contradicted by the code it cites.

- **The hit point cap cannot bite at the current balance.** `mon-make.c:1316` caps the doubled
  total at 30,000, but BAL-13's lethality scalar has already run by then: `lethality:hit-points`
  is 73 (`constants.txt:444`). The largest `hit-points` in the bestiary is Morgoth's 20,000, so
  the doubled figure is about 29,200 and the cap is never reached by anything. Writing it would
  put a number in the manual that no player can ever meet.
- **The speed cap cannot be reached either.** `:1334` caps at 199; the fastest base speed in
  the data is 160, and nightmare adds five.
- **The arrival-energy claim is backwards.** The entry says the `randint0(50)` base makes
  "twice as much" understate the divergence. The comment at `:1346-1353` says the opposite and
  is right: what was transplanted is the doubling, not the range, so a nightmare monster's
  advantage over an ordinary one is the same in both games. `nightmare.rst` is accurate as it
  stands.

**Reopen if `lethality:hit-points` in `constants.txt` is raised towards 100**, which is the
named playtest dial — at 100 Morgoth's doubled total is 40,000 and the cap starts to bite, and
nightmare.rst's multiplier becomes conditional for the deepest uniques. Original entry follows.

**What.** `docs/nightmare.rst:62, 65` states the multipliers unqualified. Monster hit points are
capped at 30,000 and speed at 199. The arrival-energy doubling works on a `randint0(50)` base
where Zangband used `randint0(100)`, so "twice as much" understates the divergence.

**Where.** `src/mon-make.c:1315` (HP cap), `:1333` (speed cap), `:1349-1354` (arrival energy).
`[read by review]`

**Chapter.** `docs/nightmare.rst:62-68`.

**Check first.** `nightmare.rst` took two tier 2 corrections (the recall branch and the ironman
claim), so the surrounding text has moved.

---

## ~~23. Two of Chaos's six changed spells are never named~~ — WRITTEN

**WRITTEN 4 October 2026** — `docs/realms.rst`, Chaos section, after the three already named.
The entry is marked `[decision log]` and the warning on it was the right one, so both claims
were traced to the data before writing. Both hold: *Call Chaos* is `effect:RANDOM` over three
fixed shapes with one element each (`class.txt:705-712`), and *Magic Rocket* is
`effect:BALL:SHARD:2` (`:715`). DEC-53 confirms the reasoning for both.

The frozen radius was written as well, with the five spells and their sizes rather than the
generic statement at `realms.rst:378`: *Flash of Light* 1, *Mana Burst* 2, *Disintegrate* 3,
*Sonic Boom* 4, *Invoke Logrus* 6, each read from `class.txt` rather than from DEC-53, and each
matching it.

**One thing to note about DEC-53 itself**, which is not a job but will mislead the next reader:
its *Summon Demon* paragraph says "4.2 has no pets ... so every demon it calls is hostile",
which pets superseded. `realms.rst` already has the current behaviour (one in three serves you).

**What.** `docs/realms.rst:380-386` says "Three are worth knowing at the table" and names three.
Not named: *Call Chaos* no longer rolls its damage type (Zangband rolled one of thirty; here the
three shapes are kept with one fixed element each), and *Magic Rocket* is now shards — so shard
resistance helps against it where almost nothing used to. Separately, the frozen radius is
stated generically at `realms.rst:378` but not in terms a player feels: the blast never grows
past its first-cast size, for *Flash of Light*, *Mana Burst*, *Sonic Boom*, *Invoke Logrus* and
*Disintegrate*.

**Where.** `[decision log]` — DEC-53. **Not traced to a code path by this review.** Before
writing, confirm the current `effect:` and `dice:` lines for Call Chaos and Magic Rocket in
`lib/gamedata/class.txt`; the shards claim in particular is the kind that would quietly change
in a rebalance.

**Chapter.** `docs/realms.rst:380`.

---

## 24. ~~The Imp lost its mutation affinity and nothing records it~~ — DROPPED, NOT TRUE

**Verified 4 October 2026 and not written, because the Imp did not lose it.**
`p_race.txt:1081` is `mutation-affinity:HORNS:7`, immediately under `name:Imp` at `:1080` — the
very line the entry cites as the race's location, one line above the affinity it says is
absent. Five races carry an affinity, not four: Beastman `POLYMORPH:2`, Yeek `SHRIEK:7`,
Mindflayer `TENTACLES:7`, Vampire `HYPN_GAZE:7`, Imp `HORNS:7`.

`docs/mutations.rst:231` **already documents it** — "an Imp grows horns", in the sentence that
carries the other four. So there is nothing missing from the docs, nothing to record in
`decisions.md`, and no drop to explain. **Reopen only if the directive is removed from
`p_race.txt`**, at which point `mutations.rst:231` becomes false and is the thing to fix.

This is the second entry in twenty-five to be false rather than stale, and both were
`[read by review]` or `[decision log]` claims of *absence* — which is the shape to distrust:
an absence is established by a grep that found nothing, and a grep of the wrong file or the
wrong spelling finds nothing too. Original entry follows.

**What.** Zangband gives an Imp HORNS 60% of the time. `lib/gamedata/p_race.txt` gives mutation
affinities to four races and the Imp is not among them (the race is at `p_race.txt:1080`).

**Where.** `archive/zangband/src/mutation.c:541-545` for the original. `[read by review]`
`[absence]` for the omission.

**Chapter.** None, necessarily — this may be a decision to record in `decisions.md` rather than
a documentation job. Flagged here because neither the docs nor a code comment records the drop,
so a later reader cannot tell whether it was deliberate.

---

## 25. Open question, not a writing job: the Vampire and Beastman affinity figures

**What.** Not an undocumented behaviour — a conflict the documentation pass should not resolve
on its own. `docs/mutations.rst:229-233` says a Vampire's gaze turns hypnotic "six times out of
ten" and a Beastman polymorphs "one time in ten". Two code comments (`src/init.c:3449`,
`src/player-mutation.c:242`) say the same. Zangband's own code
(`archive/zangband/src/mutation.c:535, 553`) produces exactly those figures, via
`randint1(10) < 7` and `< 2`.

**RESOLVED before this file was written, and this entry was stale on arrival.** The build did
use `randint1(10) <= affinity` with affinities of 7 and 2
(`lib/gamedata/p_race.txt:535` Beastman, `:824` Vampire), producing 70% and 20%. The project
owner ruled it a code defect on 28 September; it was fixed the same day in `7c971aa70` and
recorded as DEC-114. `src/player-mutation.c:284` now reads `randint1(10) <`, and the measured
rates are 0.60447 and 0.10558 over 200,000 rolls each.

Two citation faults went with it, both worth naming because they are the kind this file is
most exposed to. `src/player-mutation.c:276` was the operator's line *before* DEC-114 added
nine lines of comment above it -- the fix shifted everything past line 242 down by eight, and
this file was written from the review rather than from the tree. And `p_race.txt:784` is the
Mindflayer's `TENTACLES:7`, not the Vampire's affinity, which is at `:824`.

Nothing to write. Kept rather than deleted because the mistake it records -- a backlog entry
citing line numbers that a commit moved between the reading and the writing -- is the standing
hazard for every other entry here.

---

## Where this came from

The 28 September manual review covered 25 of the 44 files in `docs/` — everything except
`diary.rst`, `releases.rst`, `version.rst`, `download.rst` (only line 328 was checked),
`screenshots.rst`, `thanks.rst`, `copying.rst`, `meta.rst`, `start.rst` and the `hacking/`
tree. That is roughly 43% of `docs/` by line count left unexamined, mostly changelog.

**So this backlog is not exhaustive.** It is what one pass over 57% of the manual turned up. The
unreviewed pages are unlikely to hide undocumented *behaviour* — changelogs describe rather than
instruct — but `download.rst` in particular makes promises to players and only one line of it
was read.
