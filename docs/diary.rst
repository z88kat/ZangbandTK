:html_theme.sidebar_secondary.remove: true

=================
Development diary
=================

.. note::

   This is a journal, not documentation. It records what I was thinking at the
   time, including the parts I got wrong and had to undo. Where it disagrees with
   the manual, the manual is right — it gets corrected and this does not.

   Newest entries first. The :doc:`release log <releases>` says what shipped and
   when; this says why, and what it cost.

The goal
========

I played Zangband as a student. Development stopped in 2005, at 2.7.5-pre1, and
Angband did not stop — it is at 4.2.6 now, with twenty years of better level
generation, a real data-driven architecture and an object property system
Zangband never had. So: rebuild Zangband's character on top of that, rather than
resurrect a 2005 codebase.

The second goal took longer to say out loud. Zangband began as a game built on
Roger Zelazny's *Chronicles of Amber* and did not stay one. It picked up
Lovecraft, then Tolkien, then a scattering of other role-playing material, until
Amber was one theme among several rather than the thing the game was about. I
want it pointed back at the books. That is now written down as a project goal
rather than a preference, and it applies to content already imported, not just to
what comes next.


3 September 2026 — the borg is one array away from playing
===========================================================

I dismissed the borg early. Angband's automatic player looked like a novelty —
something to watch for fun — and there was a game to build. Steven came back to
it from the other end: we have no automated play testing at all, the wilderness
and ten new classes have arrived since, and the borg walks dungeons. Could it be
the test harness?

So I went and ran it, rather than reading it and guessing.

The first thing I found is that it still compiles. ``src/borg/`` is 69,632 lines
across 117 files, and it is Angband 4.2.x's borg **verbatim** — 279 commits have
touched it and not one of them is mine. The last is upstream's from 11 August,
four days before this project started. It builds warning-free against the
current headers and links into the game. Four months of changing the game
underneath it broke the compile in no way at all.

The second thing I found is that it segfaults on the first turn of every game.
``borg_update_map`` at ``ag->info |= BORG_OKAY``, which is
``&borg_grids[y][x]``. ``borg_grids`` is a **static 66 × 198 array** — Angband's
dungeon, fixed at compile time in ``borg-cave.h``. Our depth-0 level is the
wilderness surface, and ``wild_surface()`` builds it square at
``view × block_size``: ``cache-blocks:81`` gives a view of 9, block size is 16,
so **144 × 144**. The borg scans the panel, checks ``square_in_bounds`` against
the *cave*, and then indexes its own array with a ``y`` that reaches 143.

There is a guard for exactly this, and it fires on the wrong question.
``borg_init_cave`` compares its constants against ``z_info->dungeon_wid/hgt`` —
still 198 and 66, because the *dungeon* has not changed. It passes. The borg
never learns that the ground it is standing on is more than twice as tall as the
only ground it was built for.

Then I raised ``DUNGEON_HGT`` to 160, relaxed the check to ``<``, rebuilt, and
watched it play. Fourteen hundred lines of log in one short run: standing on
stairs, flowing toward down-stairs, opening a door, noticing a dropped Main
Gauche, tracking a white deer, hitting a giant white mouse, fleeing a "scary
guy", stair-scumming, then heading back to town because it was down to two
rations. Wilderness terrain, dungeon mouths, level feelings — all of it went
past without a complaint.

**That is the whole finding.** I had assumed the borg was four months behind and
would need a conversion. It is one static array behind. Everything else in the
plan is optional; that one change turns a segfault into a smoke test.

Past that it can only be a Warrior, and the reason is worth writing down because
it is the same shape as the mistake I keep finding elsewhere.
``borg_init_spell`` walks a class's spell list and matches it against a
hand-written C table **positionally, comparing names**. So M9 broke all eight
vanilla casters at once — the Mage's table wants ``Magic Missile`` at index 0 and
the game now gives it ``Zap``, and the table has 30 entries against 224 spells,
so the loop reads off the end of it too. I forced each class through
``borg_init`` in turn and got eight named mismatches, one per class.

The five classes we added are worse, and they are worse in the way that always
costs more later: they fail **silently**. The ``switch`` has no arm for cidx 9
to 13, so ``default:`` sets the ratings pointer to NULL and returns *before*
``borg_magics`` is allocated. No warning. No failure flag. If the borg had been
running when M7 landed, that would have been a one-line fix in M7 instead of a
phase of its own now.

It has zero references to pets, the wilderness, named dungeons, mutations or
virtues. That last one stings a little — ``src/borg/`` is now the largest body
of code in the tree that assumes every monster is hostile, which is precisely
what the standing PLR-22 constraint was written to prevent.

Two things I got wrong on the way. I spent a while trying to drive the borg by
injecting keypresses through ``-mtest``, and chased an abort called
"reincarnation failure" as though it were a borg defect. It is not: the borg was
activating before ``character_dungeon`` was set, because my key sequence never
actually finished the birth, and the number of keys birth wants depends on the
roll. The harness was the bug. That is why the plan's first real requirement
after the array is a proper headless entry point rather than a cleverer script —
keypress injection is not a foundation. And I put ``borg.txt`` in two wrong
directories before finding the right one, which is its own small argument for
having the build install it.

The plan is DEC-66 and §7 of the Phase 2 development plan: five phases, B0 to
B4, twenty-one requirements. The rule I care most about is in the decision
rather than the plan. Eight rewritten C tables would fix the casters and cost us
a permanent conflict with a subsystem upstream is still actively maintaining.
Two hundred spell names rated once in ``lib/gamedata``, keyed by name and realm,
fix it and cost nothing — and a new realm after that is free. DEC-11 traded away
merging and kept cherry-picking; this is one of the places where remembering
that trade changes the design.

One last thing, and it is the argument for the whole exercise in miniature.
While forcing classes through ``borg_init`` to see which ones failed, the
Mindcrafter reported **zero spells**. I have not chased it yet and it is a game
question rather than a borg question. But nothing else had noticed, and the
thing that noticed was a test harness that does not work yet.


3 September 2026 — SDL 3, and the front end that is already not what it says
=============================================================================

Steven asked what it would take to move to SDL 3.4, and whether anything would
break. The honest answer to the second half is what decided the first.

I started by counting. The SDL 2 front end is ``main-sdl2.c`` at 8,502 lines, the
``pui`` widget toolkit under ``src/sdl2`` at another 7,094, and ``snd-sdl.c`` at
293. Two hundred and twenty-eight distinct SDL symbols. I made a list of every
symbol SDL 3 renames or removes and counted the occurrences: **731**, and that
is a floor — it excludes the ``SDL_Rect`` → ``SDL_FRect`` conversions the float
renderer forces, and every ``event.key.keysym.sym`` that becomes
``event.key.key``.

Then I diffed the toolkit against upstream Angband, and that is where the
question actually got answered. ``pui-ctrl.c``, ``pui-dlg.c`` and ``pui-misc.c``
are **byte-identical** to upstream. ``main-sdl2.c`` differs by 677 lines, which
is essentially the Emscripten work and nothing else. Fifteen thousand lines that
somebody else maintains, and they are still maintaining them: the recent commits
on those files are ``SDL2: check for allocation failures from SDL_strdup()`` and
``SDL2: add missing checks for memory allocation failures``.

DEC-11 gave up merge compatibility and deliberately kept cherry-pick
compatibility, on the argument that cherry-picking needs only that a file "still
exists and is recognisably related", and that the value it captures is exactly
"crashes, leaks, portability, undefined behaviour". Those two commits are that
list, verbatim. Porting to SDL 3 would not bend that rule, it would spend the
thing the rule was written to protect — and spend it on the one part of the
codebase where we have contributed almost nothing and gain almost nothing by
owning.

The web build settled it independently. Emscripten's SDL 3 port first appeared
in emscripten 5.0.0 this January, ``sdl3_ttf`` in 5.0.3 in March; we are pinned
to 3.1.51, so that is a four-major-version toolchain jump underneath a build
whose whole existence depends on Asyncify behaving. And there **is no**
``sdl3_image`` port. Only ``sdl3.py`` and ``sdl3_ttf.py`` exist. Our tilesets go
through ``IMG_Load``, so the browser build would need SDL3_image built from
source under ``emcmake`` and linked by hand, or it loses tiles. The AppImage has
the same shape of problem from the other end: it is built on ubuntu-22.04 on
purpose, for glibc, and ``libsdl3-dev`` is not in 24.04 let alone 22.04.

So: no. We stay on SDL 2, and that is DEC-65.

What the investigation actually turned up was the other front end. ``main-sdl.c``
is 6,169 lines of SDL **1.2**, still built by CI, shipped by nothing. I had been
thinking of it as harmless. It is not, quite: Debian and Ubuntu have replaced
``libsdl1.2-dev`` with **sdl12-compat**, a shim that reimplements the SDL 1.2 API
by dlopening SDL 2. Fedora and Arch did the same earlier. Which means the job in
``linux.yaml`` labelled *SDL* is not testing SDL 1.2 at all any more. It is
testing SDL 2, reached through a translation layer, in a front end nobody runs,
against a second copy of the sound backend.

I went looking for what we had changed in it, expecting to find a reason to keep
it. One hunk: the About box, made to print every credit line instead of the
first, to match ``main-sdl2.c``. That is the whole of our investment.

There is something slightly funny about spending a day establishing that we
should not move from SDL 2 to SDL 3, and finding at the end of it that the real
work is moving from SDL 1.2 to SDL 2 — a migration the distributions have
already performed on our behalf, without asking, and which we have not noticed
because the build still goes green.

3 September 2026 — thirty-five savefiles, and the difference between losing a book and inventing a priest
=========================================================================================================

The entry above this one, from 30 August, said the savefile format was fine and one
function had never been brought into line with the rest. That was true and it was not
the whole thing, and this is the rest of it.

It was never versioning. A savefile records what it holds **by name** — ``prayer book``,
``[Novice's Handbook]`` — so when DEC-50 replaced Angband's five prayer books with
Zangband's four, every save in the corpus named an object the game no longer had. And
every save carries the town temple's stock, and the temple sells prayer books. No version
number helps with that. The bytes parsed perfectly and then asked for something that does
not exist.

What turned one missing book into a dead file was a return value doing two jobs.
``rd_item()`` returned ``NULL`` both for *the list ended* and for *I cannot name this* —
and the end-of-list marker is itself an item with no kind, ``wr_item``'s dummy. So a reader
that met a vanished object could not tell which had happened. The ones that guessed
"ended" stopped early and left the rest of the list unread, the stream desynchronised, and
everything after it was noise. It now returns ``lost`` alongside, the record is always read
to its end, and the artifact and ego failures no longer return mid-record — that last part
is what had made this unrecoverable rather than merely lossy.

Then ``rename.txt``, consulted only after a lookup has already failed, so a current save
never reaches it. Twenty book titles from the four realms DEC-50 replaced, and the two
monsters DEC-30 dropped. The trace that had shown 187 lost prayer books, 186 sorcery and
126 shadow now shows zero.

The corpus went from **every file refused** to 31 loading with everything intact and 4
refused. The four are casters, and they are refused **on purpose**. Their spells are
recorded by flat position and DEC-50 moved them, so there is no honest way to read them.
That is the rule this settles on, and I want it written down because it is the kind of
thing that gets eroded by a well-meaning fix later: *content can be dropped, identity
cannot be invented*. A missing book is visible the moment you check your pack. A Priest
quietly holding somebody else's spell list is not.

The character is told once — "8 things went missing while you slept" — counted rather than
announced item by item, because one removed kind can take every copy on every shelf in the
world.

Two things worth keeping from the testing. The corpus cannot catch a change that stops it
loading, because by then there is nothing left to test with; so ``game/roundtrip`` now
saves a character, renames a kind it is carrying **in the live table**, and loads it twice
— once with no rename entry, where the object is lost and the character is fine, and once
with one, where it comes back. Both break tests passed first time for a bad reason: they
renamed the kind before ``reset_before_load()``, which runs ``init_angband()`` and rebuilds
the object table from the data files, so the rename was undone and the test passed having
broken nothing.

And one real bug on the way: ``obj->sval`` is a ``uint8_t``, so ``lookup_sval()``'s ``-1``
lands in it as 255 and ``obj->sval < 0`` is dead code.


3 September 2026 — Trump, and the realm that was waiting for a side to be on
=============================================================================

DEC-54 deferred Trump whole back in 3.55.0, and the reason was not difficulty. Fourteen of
its thirty-two spells summon a creature that serves you, and the game had no side for a
monster to be on. Importing them as hostile summons would have turned a realm whose theme
is *you deal the cards and the cards fight for you* into a realm that fills the room with
enemies. So it waited for PLR-22. This is it arriving, and all seven realms now have books
behind them — ``player/realm`` asserts zero empty realms rather than one.

Before Trump could arrive, pets had to come from somewhere, and that was the phase before.
Two mechanisms carry all of it: charming something already there, and summoning something
that turns up on your side. Charming is three separate projections — ``MON_CHARM``,
``MON_CHARM_ANIMAL``, ``MON_CHARM_UNDEAD`` — because Zangband has three, and each has a
different notion of what it can work on, in realms that are genuinely different realms:
persuasion is Life's, animals are Nature's, the dead are Death's. ``NO_CONF`` refuses the
general charm only, because taming an animal and commanding a corpse are not persuasion.

The nicest find in that phase was in ``summon_specific()``, which needed one line on
``cave->mon_current`` — a field 4.2 already uses two lines earlier to put a summon in its
summoner's group. A player's summon is hostile unless the caller asks otherwise, and the
machinery to know whose summon it is was already sitting there.

On Trump itself, one rule runs through every summon. Zangband writes ``bool pet = success``
in all of them, with the comment ``/* was (randint1(5) > 2) */`` beside it — somebody had
a dice roll there and deliberately replaced it with certainty. So a Trump summon that goes
off is a pet, full stop, and the angry version is the *failed* casting, which reaches the
miscast machinery the same way Death's and Chaos's do. The tests pin twelve summoning
spells per fully-entitled class and **zero** plain ``SUMMON`` in the realm, because a
single one mapped the old way would look right in every other test.

Five spells are deferred, and I made a point of giving each its own reason rather than
filing them behind a shared wall. *Shuffle* is a deck of many things — one d120 read off
about twenty **unequal** bands, and 4.2's ``RANDOM`` picks uniformly, so even the outcomes
I can express could not be weighted; the whole character of the spell is that the bad
results are common. *Reset Recall* writes a recall depth the game does not offer.
*Dimension Door* lets you choose where you arrive, and ``TELEPORT_TO`` does not, so mapping
it there would make it Teleport, which the realm already has two spells earlier. *Joker
Card* summons one of five ``SUMMON_BIZARRE`` groups, and 4.2's summon table is built round
threat categories rather than round the joke. *Trump Lore* is ``identify_fully()``, the same
wall Sorcery's Identify True hit. They are named in a test so a sixth cannot join them
quietly.

Two smaller things this phase found, both in the tooling rather than the game. The
converter's book-line pattern was ``^book:\S+(?: \S+)? book:`` — every realm before Trump
had a book-noun ending in the word *book*, and Trump's is a **deck**. So its books were
invisible to the converter, the realm before Trump in each class ran on through them, and
the checker reported correctly-emitted classes as broken. And ``TransferLib``, which stages
``lib/gamedata`` for the unit tests, is a ``cmake -E copy_directory``: it compares
timestamps and can decline to copy a file that was reverted within the same second it was
staged. Two falsifications read a stale copy before I understood that.


3 September 2026 — what a pet costs, and what it will not do
=============================================================

Three phases, and the theme running through them is that Zangband's documentation was right
about the design more often than its code was clear about it.

**Orders are a policy, not instructions.** PLR-25 asks for "nine command modes with
per-mode distance behaviour". What Zangband has is nine menu entries, of which five are
modes: five leash lengths, two toggles, one report, one dismiss. All nine are built. And
the orders live on the *player*, not on each animal, which is the right shape — you are
setting how your creatures behave, not telling each one what to do. The leash is signed and
the sign is the meaning: positive is "stay within", negative is "keep at least this far
away, and do not pick fights nearer the player than that". It goes to the savefile as
signed 16-bit, with a test that sets it negative, because read unsigned that is 65511 — a
legal-looking leash no order can produce.

The command key is ``A``, not Zangband's ``p``. ``p`` is auto-explore here and in use.

**Pets do not follow you downstairs, and that is a real disagreement with the requirement.**
PLR-26 says pets "persist across level changes and saves, following the player where the
mode implies it". The saves half is true and tested. The level-change half is an inference,
and I went looking for what it was inferred from. There is **no pet-carrying code anywhere
in Zangband 2.7.5** — ``change_level()`` unreferences the region and the monsters go with
it, and searching both archived lineages for the mechanism Hengband added later
(``party_mon``, ``preserve_pet``) finds nothing. And the documentation never mentions it: it
explains the upkeep, the killing-blow rule, that pets trample you, that they anger easily,
and every way of getting one. Taking one downstairs does not come up.

So a pet is a per-level asset. That is coherent, it is Zangband's, and it is most of the
answer to "does a summoner trivialise the game" — a stable has to be rebuilt every level.
I flagged this one for Steven rather than deciding it alone, because it is a genuine gap
between the requirement as written and the game as built, and adding following later is
contained but it is new design under DEC-30, not a port.

**Upkeep is a count, and then a cliff.** This is the one I nearly got wrong. A count is
free — ``1 + level / pet_upkeep_div`` pets — and past that count the **sum of the pets'
levels** is the percentage of mana regeneration withheld, clamped to 5..95. The charge is
not per-pet-over-the-limit. One pet over the allowance turns the meter on for the *entire
stable* at once, and a summoner's third animal can cost more than the first two together.
That edge is the balancing pressure, and an implementation that charges only for the excess
is a different game. The falsification produced 92% where the rule gives 95%, which is
exactly the size of error that would have shipped.

Zangband weights each pet by ``hdice * 2`` where its documentation says "the sum of the
levels of your pets". Across its 883 monsters those are equal for 48% exactly, median
difference zero, within two for 96% — the same number written twice, with the outliers all
deliberately weak-for-their-depth creatures: leprechauns, memory moss, the quantum dot. So
``race->level`` is the faithful port, and unusually the prose was right where the code
looked wrong.

One place I departed from the source on purpose. The charge multiplies mana **gains** only.
A Blackguard's ``PF_COMBAT_REGEN`` makes ``sp_gain`` negative — it burns spell points rather
than restoring them — so Zangband's unconditional multiply would mean a stable of pets
*slowed the burn down*, paying the player for the thing the mechanism charges for. Measured:
15 points burnt over a hundred turns alone, 1 with the multiply applied. Zangband had no
class that loses mana by design, so it was never wrong there. Ours would have been.

**And walking into your own animal swaps places with it.** PLR-24 asks for "confirmation
before harming a pet". Zangband does something different and better: you push past it,
and there is no prompt anywhere in its pet handling. The reasoning holds up — the danger is
not that a player decides to punch their own wolf, it is that the wolf steps into the
doorway on the turn you were walking through it. A prompt on that step would fire constantly
and train the player to answer it unread. The exceptions are Zangband's list, and each one
is a way of not being in command of yourself: confused, hallucinating, stunned, berserk, or
unable to see what is there. Its Stormbringer clause — a one-in-three chance the sword
swings anyway — has no equivalent here and I did not invent one.

Anger goes in one place rather than seven. ``mon_take_hit()`` is *the* player-caused-damage
entry point in 4.2 — melee, missiles, every projection — and monster-caused damage goes
through ``mon_take_nonplayer_hit()`` instead. Zangband had to call ``anger_monster()`` at
each site because its damage path had no such split, and the sites it missed are why a
player there could drop a wall on a pet for free. Both halves are tested, because a pet must
*not* blame you for a hostile monster's fireball.

Aggravation had to move. 4.2 reads ``OF_AGGRAVATE`` inside ``monster_reduce_sleep()``, which
only a sleeping monster reaches — and since PLR-23 no ally is ever asleep, so the rule would
have been unreachable for exactly the monsters it is about.

The virtue writes came across too, and they are worth a note. Zangband changes four virtues
when an ally turns — Individualism up, Honour, Justice and Compassion down — and they were
dead numbers there, because it had no consumer for any virtue. Here they are live under
PLR-21, so turning on a creature that trusted you now reaches a Chaos patron's generosity
and what you dream about at an inn. Something written and unused for twenty years started
working.

*A note on process, again.* The confusion test passed against a deliberately broken build.
It asserted "they are not on the same grid" and "one of them moved" — both true after a
swap as well as after a failed swap. The code was right and the test was not, and only
running the falsification showed it.


3 September 2026 — a monster can be on your side
=================================================

M10, and the invariant the whole milestone rests on: a monster is not necessarily an enemy.
The plan has said since M0 that nothing before M10 should deepen the assumption that it is.
This is the phase that spends that.

The requirement document names the wrong mechanism, which is worth recording because it is
the second time. §2.6 cites ``RF6_FRIENDLY`` with an ``is_pet()`` predicate; ``RF6_*`` is
Zangband's *spell* flag set and has nothing to do with allegiance. What it actually is: two
bits stolen inside ``m_ptr->smart``, the smart-learn bitfield, both carrying an ``/* XXX */``
marker in ``defines.h``.

We take the states and leave the encoding, and the reason is a bug in the original.
``set_pet()`` was ``m_ptr->smart |= SM_PET`` and never cleared ``SM_FRIENDLY``;
``set_friendly()`` was the mirror image. Both bits could stand at once, and the monster
behaved as whichever predicate happened to be tested first — which is ``is_pet()`` almost
everywhere. So a pet the player deliberately released kept taking orders and kept costing
upkeep. One enum field assigned through one setter cannot reach that state.
``MON_ALLEGIANCE_HOSTILE`` is zero, so every existing creation path — generation, breeding,
shapechange, the townsfolk — keeps producing hostile monsters without being told to.

4.2 has no ``RF_GOOD``, and ``are_enemies()`` needs one, because Zangband checks alignment
*before* sides: a good creature and an evil one fight whatever side either is on, including
two of the player's own pets. Without the flag that rule reads half a table and never fires.

That found a data bug I would not have gone looking for. 4.2 gives monsters a ``base:``
template whose flags merge into the race's, and the ``dragon`` and ``ancient dragon`` bases
carry ``EVIL``. So the law drake and the Great Wyrm of Law — GOOD and *not* evil in Zangband
— came out GOOD **and** EVIL here, which made each of them an enemy of both alignments and
of nothing else. Balance, not Law. The balance drake keeps both flags because Zangband gives
it both deliberately, and a test pins the difference in both directions so a later tidy-up
cannot flatten one into the other.

PLR-27 asks for pets to be "visually distinguishable", and it turns out that is a word, not
a colour. Zangband's ASCII distinction was the look string ``" (pet) "``; ``MONST_PET`` and
``MONST_FRIEND`` were flags for its graphical Tk client and its borg, never for the map. So
we annotate look and the monster list, in the same shape as the sleep tag, and leave the
glyph alone — recolouring would have to beat three attr rules already in place (multi-hued,
purple uniques, shapechangers) and would lose to all three on the monsters most worth
identifying. The one change from the original is putting the word *first* rather than after
the health and the recall prompt, because that is where a player reading a crowded floor
stops.

Then phase 2, and the pleasant discovery that 4.2 was much closer to this than the survey
said. The requirement's conclusion was that "every place 4.2 assumes monster ⇒ enemy is a
potential defect site" — true of the goal code, and **not** true of the combat code, because
4.2 already carries a complete monster-versus-monster path built for the Necromancer's
``MON_TMD_COMMAND`` power. ``monster_attack_monster()`` resolves blows with the same effects
the player takes, ``mon_take_nonplayer_hit()`` awards no experience and leaves uniques at one
hit point, ``do_mon_spell()`` already rolls against a target monster's armour class when
``mon->target.midx`` is set, and the bolt and ball handlers already aim there. Phase 2 reuses
all of it and writes none of it. PLR-31 turned out to be very nearly already true.

Allies get their own branch of ``get_move()`` rather than a substituted target, and that
distinction matters more than it sounds. Everything ``get_move()`` does after choosing a
target is about the *player*: it walks the noise and scent heatmaps flowing out of the
player's grid, it flees *from* the player, and its pack AI works to surround the player and
pull them out of corridors. Swapping the target grid would leave a pet fleeing from its
owner and trying to surround its own enemy with a pack that is not there. Zangband split it
the same way.

An ally is always awake, because all six of 4.2's activity tests measure the player — can it
see, hear or smell them, is it hurt, is it burning — and an ally satisfies none of them while
standing next to something it should be fighting. It would sleep through the battle.
Zangband got to the same place from the other direction, waking *every* monster on the level
whenever the player has pets at all; ours is the narrow version, and the test pairs a pet
with a hostile monster at the same distance so it cannot pass by waking everything.

One ordering choice that is not cosmetic: the enemy check goes **before**
``monster_turn_try_push()``. That function's ``monster_can_kill()`` lets a monster with
``KILL_BODY`` walk over a weaker one and delete it outright — so a pet standing between the
player and something large would simply stop existing, with no blows, no message and nothing
to react to.

Two things kept from the source deliberately. Target selection takes the **first** qualifying
monster scanning backwards rather than the nearest, because Zangband's comment says newer
monsters tend to be closer and the effect is that a pack of pets spreads across several
enemies instead of converging on one. And a remembered target is kept while it still
qualifies, so a pet does not abandon a wounded enemy every time something fresher walks in.

One limitation recorded rather than fixed: ``remove_bad_spells()`` filters a monster's spell
list against what the **player** is known to resist. For a pet casting at a monster, that is
aimed at the wrong creature. It makes a pet slightly worse at choosing spells and never
wrong about the result, so it waits until there is a reason to touch it.


30 August 2026 — every old character stopped loading, and it was not what I thought
====================================================================================

Steven put thirty-five of his savefiles into ``tests/saves`` and said, reasonably,
that not breaking them matters. I wrote a suite that loads every one of them. It
failed on the first file, and then he told me the app itself terminated when he
tried to open Amanwe. So this was not a test being fussy — it was every character
he had ever played, gone.

My first diagnosis was wrong, and wrong in an expensive direction. I saw that the
``misc`` savefile block writes arrays sized by counts that come from the data
files — how many object flags, elements, brands, slays, curses there are — and
that four commits the day before had changed exactly those counts. I concluded
the block had no version mechanism, that the format was structurally fragile, and
that the fix was to version it and keep a reader for the old layout. I said so
with more confidence than I had earned.

The counts are already in the savefile. Every one of them is written out and read
back during the object-memory block, into file-static variables the later readers
use. ``rd_item`` uses them. ``rd_ego`` uses them. ``rd_misc`` does not — it loops
to ``OF_SIZE``, ``OBJ_MOD_MAX`` and ``ELEM_MAX``, the values compiled into *this*
build, and reads past the end of anything written by a build with fewer. Three
identifiers. The format was fine; one function had never been brought into line
with the rest of the file, and nothing had ever changed a count before, so nobody
found out.

That is the second time on this project that a structural explanation turned out
to be a typo, and both times the structural explanation was mine. It is a
comfortable kind of wrong: it makes the problem important and the fix large, and
it does not require reading the neighbouring function.

Fixing it moved the failure rather than ending it, which is how I found the other
three. All of them are the same defect wearing different clothes — *the savefile
names something this build no longer has*:

``rd_monster_memory`` skipped the read that advances its loop when a monster had
been removed, so it span on the same name, four bytes at a time, until it fell
off the end of the buffer. ``rd_monster`` treated a vanished race as fatal to the
entire load — and that is what actually killed most of the corpus, because
``raving lunatic`` and ``hobo`` do not exist any more and eleven characters had
met one. ``rd_trap`` dereferences the trap kind it just looked up without
checking it, and ``rd_ignore`` calls ``quit()`` outright over an autoinscription
on an object kind that has been renamed. Those last two have not fired yet. They
are waiting for the first trap or object I rename.

The monster fix had a trap in it I nearly walked into. Dropping a monster leaves
a hole in the array, and a hole loads perfectly well — it crashes the *next save*
instead, because ``wr_monster`` reads the race of every slot from 1 to
``mon_max``. So the survivors are renumbered as they are placed, carrying their
held objects and mimic back-references with them. The suite now loads each
character, saves it to a scratch file and loads it again, because loading alone
would have passed and left that waiting.

Thirty-five of thirty-five load. What I am keeping from this is not the fix. It
is that I had a working diagnosis, a plausible mechanism and a plan, and the
actual bug was three identifiers away in a function I had read past twice.

Then, the same afternoon: *the white deer is still stalking me.*

I had fixed that three days ago, and the fix was working. The deer gave its
blessing once and shied away from every touch after; the character's memory of
it held; the tests said so. What I had never asked was whether the beast
actually *left*. It bounded ten grids, and I had a careful comment justifying
ten — measured, tested, the worst of thirty bounds is nine. All true, and beside
the point. A white deer moves at speed 130 and hears at 40. Ten grids is not a
departure, it is a pause. It came straight back and stood there being refused.

The number was right for the question I asked and the question was wrong. I had
measured how far it went and never once measured whether that was far enough to
matter, which is the only thing the player experiences. The bound now has to
clear the beast's own ``hearing`` — the test asserts against ``race->hearing``
rather than a constant, so it is checking the property that makes it work rather
than the number that happened to.

Two fixes in one day, and both of my earlier answers had been confident,
documented and slightly beside the point.


27 August 2026 — the game in a browser tab, and four things that were wrong
===========================================================================

ZangbandTK runs in a browser now, at `zangbandtk.com/play <https://zangbandtk.com/play/>`_.
Not emulated and not on a server: the same C, compiled to WebAssembly, executed
by the browser directly. I had assumed this would be the hard port, after the
Nintendo DS. It was the easy one.

Every source file compiled to wasm on the first attempt — the core, the borg, all
eight thousand lines of the SDL2 front end — and the repository's own test suite
passed against the wasm binary through node before I had looked at a single pixel.
That last part turned out to matter more than anything else I did today: it
separated "does the game work" from "does the *drawing* work", and every real bug
after that was in the second half.

Two things I expected to break did not. The packaged fonts are Windows ``.fon``
bitmaps opened through ``TTF_OpenFont``, which means FreeType's WinFNT driver has
to exist in the Emscripten build of SDL2_ttf; it does, with correct metrics. And
the front end blocks while waiting for a key — it spins on ``SDL_Delay`` until
one arrives — which in a browser is a tab that never returns to its event loop
and so never receives the key it is waiting for. Asyncify rewrites the compiled
binary so a blocking call can unwind and resume, and Emscripten's SDL2 already
maps ``SDL_Delay`` onto it. The one thing I was sure would need rewriting needed
a compiler flag.

Then the four that were wrong.

**A home directory that was not there.** Emscripten defines ``__unix__``, so
``h-basic.h`` defined ``UNIX``, so the game went looking for ``~/.angband`` and
quit trying to create ``/ZangbandTK``. DOS had already been excluded from that
branch for the same reason years ago; the browser joins it.

**The ``=`` key did nothing.** This is the one I want to remember, because I
reasoned my way to the right answer, talked myself out of it, and then had to
measure. The front end routes ``=``, the digits and ``- + . / *`` through the
keydown handler rather than through text input, and drops them from text input to
avoid handling them twice. So ``=`` going missing looked like exactly that
suppression — except the digits worked, which seemed to rule it out. It did not.
On this keyboard ``=`` is Shift+0, and the shifted half of that handler knows two
keys in total and says as much in a comment: "Does not match every keyboard
layout, unfortunately." No match, nothing produced, and then text input arrived
with a perfectly good ``=`` and threw it away. The suppression's own comment says
it should drop a character *if the keydown handled it*; it never checked. It does
now, and this is not a browser bug — it is every non-US layout on desktop SDL2
too, and it should go upstream.

**Fullscreen jumping in and out.** Removing the menu entry did nothing, because
the cause was the window being *created* with ``SDL_WINDOW_FULLSCREEN_DESKTOP``.
Emscripten turns that into ``emscripten_request_fullscreen_strategy`` with its
defer flag set, so the page leapt to fullscreen at the player's first keystroke
and fell back out at the next Escape, forever.

**A window nought pixels wide, which was mine.** Stripping that flag also skipped
the branch just underneath it, the one that swaps in the stored fullscreen size —
so the window took a windowed size that had never once been used, because the
window had always been created fullscreen. Zero. Fatal before anything drew, on a
config written by an earlier build of the same afternoon. The lesson is not about
fullscreen: it is that a saved size cannot be trusted in a page at all. It takes
the viewport now and ignores what was stored.

The browser version is deliberately the smaller game. No sound — compiled out,
which also keeps three megabytes of samples out of the download, and nobody wants
this firing out loud in an office. One terminal, because a page has one canvas and
no way to ask for a second, so the buttons that would have opened the message and
inventory windows are gone rather than present and inert. No fullscreen. Three
tilesets instead of five. Eight megabytes to start, once.

It publishes from the same workflow as this manual, which is not tidiness but
necessity: a Pages deployment replaces the whole site, so a second workflow
publishing only the game would take the documentation down, and the reverse.
One artifact. And because it builds from master rather than from a tag, ``/play/``
is now the newest ZangbandTK in existence and the least settled — which is the
right trade for something you reach by clicking a link.


29 August 2026 — an empty sea, and a pointer that outlived its data
===================================================================

Steven walked the coast looking for the fish I had just fixed and found none. He
was right, and I had shipped a half-fix: I gave the aquatic monsters a proper
base and put them in two dungeons, and never once asked whether anything spawns
in the wilderness sea.

Nothing does. Nothing ever did. ``wild_populate()`` asks ``square_isempty()`` of
each grid, that calls ``square_isfloor()``, and neither depth of water carries
the FLOOR flag — so every square of ocean in the world was refused, for every
monster, since the day the wilderness was written. My own comment on the line
even said "not in the sea or the fire", as though it were a decision.

The fix is two passes, land and water, each with its own filter. Then three
things in a row that were each individually reasonable and collectively made the
sea useless:

The **density** is read from the block's population, and population measures what
the land supports. Open water scores near zero, so the ocean was the emptiest
place in the world — twelve times emptier than farmland. It has its own figure
now.

The **danger** is derived from law, and law measures how well the country is
policed. Nobody polices the sea. A calm bay off a lawful city came out at danger
three, and the shallowest fish in the game is a swordfish at eight, so those
waters were empty however long you swam in them. There is a floor now, and it is
safe to walk past because fish cannot come ashore.

And they *could* come ashore, at first. A shoal arrives through
``place_friends()``, which scatters its members around the leader without
knowing or caring what they are, so one piranha in ten ended up flapping on the
beach. That check belongs in ``place_new_monster_one()``, where every path goes
through it — summons and escorts have the same shape.

The bug that cost the most, though, was mine and was three lines long:

.. code-block:: c

   static struct monster_base *fish = NULL;
   if (!fish) fish = lookup_monster_base("fish");
   return race && fish && race->base == fish;

A perfectly ordinary cache. The monster data is freed and reparsed whenever the
game reloads it, so the cached pointer outlives the thing it points at and then
matches nothing — the filter accepted fish on the first call and none after. I
watched an allocation table with two hundred eligible entries report zero, three
times, before I thought to ask what was being compared rather than what was doing
the comparing.


29 August 2026 — sharks in the forest of Arden
==============================================

I set out to give the imported monsters tiles and found a barracuda borrowing a
tree's picture. That was not the tile script being stupid. The barracuda has
``base:tree``.

Zangband drew aquatic monsters with ``l``. Angband 4.2 draws trees with ``l``.
Whoever did the import carried the glyph across and the base came with it, so
every fish, shark, whale, squid, seahorse and kraken in this game — twenty-four
of them — has been a tree.

I nearly filed it as cosmetic. It is not, and the reason is one line in
``mon-init.c``: ``rf_union(r->flags, r->base->flags)``. A base's flags are
inherited by every monster wearing it, and a white shark declares nothing but
``ANIMAL``. So every one of them has been **immune to fear, immune to confusion,
and regenerating**, and nobody could have worked out why. Then, because dungeon
dwellers are matched by base, Arden — Zelazny's forest, the one Corwin rides
through — has been spawning great white sharks. The Grove of the Unicorn too. The
Forest monster pit could fill with krakens.

The fix is small: a new base, twenty-four records changed, a bestiary category.
The ents stay trees. And Faiella-Bionin, the stairway that runs down beneath the
sea, and Rebma, the drowned city, now have fish in them, which they should have
had from the day they were written.

The glyph was the only real decision and there was almost nothing to decide with.
``N`` is the last free letter in the entire game. ``~`` is the obvious choice and
the worst available: it is the chest mimic's glyph and it is the water these
things swim in, so a shark would be invisible against the sea.

Two things I want to remember. The first is that this was found by accident,
while doing something else, by looking at one odd-looking output line — and I
had been about to explain it away as a quirk of my matching heuristic. The
second is the shape it shares with the powers that did nothing: the data parsed,
every field was valid, every number was Zangband's own, and the meaning was
wrong. Three times this week now. Valid is not correct.


29 August 2026 — a review, and five things that did nothing
===========================================================

Ran a review over the whole of M7 — about five thousand lines of new mechanism,
the races and their powers, martial arts, psionics, patrons, a new projection.
Fifteen findings. One I rejected, three belong to Steven's graphics work rather
than mine, and eleven were real.

Five of them shared a shape, and it is a shape worth naming. A Yeek's scream did
nothing. A Sprite's sleeping dust did nothing. A patron's *destruction* levelled
exactly one grid — the one the player was standing on. A Draconian's breath was a
needle at a fifth of its range. Two of the Mindcrafter's level bands were the
same band. In every case the character paid the full price, the game printed the
message, and nothing happened.

They are all the same mistake: a number in the wrong slot, or no number at all.
``effect_calculate_value()`` returns zero for an effect with no dice, and for a
projection zero does not mean "a little" — it means "nothing". A ball with radius
zero is quietly given a radius of two, so my two carefully banded Pulverise
entries were identical. ``BREATH:FIRE:20`` puts the 20 in the radius slot, not
the arc slot, because that is the parameter order; every other breath in the game
data is written ``BREATH:FIRE:0:30`` and I did not look at one.

None of this is visible from inside the game and none of it was caught by a test,
because my tests all checked that the data *parsed* and that the levels and costs
were Zangband's. Parsing is not the same as working. There is now one test that
walks every power on every race, every power on every class, and every rung of
every patron's ladder, and fails if an effect that means nothing without a value
was not given one. Reintroducing the Yeek bug makes it say so by name.

The structural one was worse. A patron's reward is handed out on gaining a level,
and two rungs on every ladder grant or drain experience — both of which call back
into the very loop that was calling them. A kill worth several levels at once
could recurse, hand out a reward per re-entry, and announce the same level twice.
Zangband deferred the reward out of the loop and I had not wondered why. Now I
know why.

The lesson I want to keep is the one about parsing. Four days of building
data-driven mechanisms, and my instinct each time was to test that the file
loaded and the numbers matched the source. Not one of those tests would have
noticed that half the powers did nothing when used.


29 August 2026 — a screenshot that was a bug report
===================================================

Steven sent a screenshot of a Sprite Mindcrafter to go in the manual. It is a
nice picture: the power list open, twelve psionic powers and the race's own
sleeping dust, the village outside. I nearly just cropped it and wrote a caption.

The header line reads *"You are a Sprite Mindcrafter, and pay for this out of
your own hide."* — which is a sentence I wrote, for the case where a character
has no mana at all. A Mindcrafter is a caster. It should never have seen it.

``calc_mana()`` reads the weight of armour a character may wear before mana
starts draining out of ``p->class->magic.spell_weight``. A Mindcrafter has no
magic block, so that is zero, so *all* its armour counts against it — and its own
starting soft leather weighs eight pounds, which cancels eight points of mana
from a class that has about two at level 1. It began the game with nothing to
spend and stayed that way, paying for every power in blood, for fifty levels.

I had written a test for exactly this, and the test passed. Twice, in fact:
it passed before the fix and after it. It checked a Mindcrafter at levels 20 and
50, where losing eight points of a large pool is invisible, and it never put any
armour on the character. Making it dress at level 1 made it fail immediately,
with 0 mana where 1 was needed — and I only checked that the test could fail by
putting the broken line back and watching it go red, which is a habit I should
have already.

Then the same assumption again, one layer up: the sidebar would not draw the SP
row, because ``prt_sp`` also asks whether the class has spellbooks. So the mana
existed, and nothing on screen said so. Three places were guessing at "does this
character have mana" from "does this class have books", which used to be the same
question and stopped being one the moment PLR-06 landed. It is one function now.

The screenshot is in the manual, regenerated from the fixed build, and it says
*"and have 0 of 2 spell points"*. Worth noting for its own sake: a player's
screenshot was a better bug report than my test suite, and the bug was three days
old.


29 August 2026 — nine Lords, and a test that measured the wrong thing
=====================================================================

The Chaos-Warrior, which completes every class in M7 that is not waiting on the
realm system. It is the only class in the game that belongs to somebody: sworn at
birth to a Lord it did not choose and cannot leave, and every level it gains the
Lord looks up and decides how it feels.

The interesting part was not the mechanism. It was the roster.

Zangband's sixteen patrons are Slortar, Mabelode, Chardros, Hionhurn, Xiombarg,
Pyaray, Balaan, Arioch, Eequor, Narjhan, Balo and Khaine — Moorcock's Elric gods
— plus Khorne, Slaanesh, Nurgle and Tzeentch, who are Warhammer. Not one of them
has anything to do with Amber. It is the single clearest example of the drift
this whole project exists to undo, and it would have cost nothing to import all
sixteen and notice in a year.

So: nine Lords of the Courts of Chaos instead. Swayvill, Suhuy, Mandor, Dara,
Gramble, Jurt, Despil, Borel, Gilva. And I went and checked them rather than
writing down what I remembered, which was the right call, because I had two of
them wrong — I had Suhuy in House Hendrake and Dara in Sawall, and they are
Sawall and Helgram respectively. Three more names I was fairly confident about,
Tmer, Tubble and Bances, are not in the reference at all, so they are not in the
game. DEC-18 says facts get rigour and a patron roster is a fact.

The rest was routine, apart from one thing worth writing down about testing.

I wanted to pin Zangband's nicest piece of malice: the odds of your patron
turning cruel are normally one in six, but on reaching level 13 they are one in
two. It is nowhere in the interface — the only way to learn it is to live through
it — so it would survive being silently lost, which is exactly what a test is
for.

My first attempt ran four hundred level-ups at 13 and four hundred at 20 and
counted how often the character lost hit points. It passed: 73 against 58. And it
was a bad test, for two reasons I should have seen before writing it. Most of the
cruel outcomes do not cost hit points at all — a cursed weapon, a drained stat,
a summoned pack — so it was blind to most of what it claimed to measure. And
several of the *kind* outcomes recalculate maximum hit points, so it was counting
things that were not damage. Fifteen apart in four hundred is noise wearing a
result's clothing, and I would have shipped it.

What it should have measured was the roll, so I gave the roll its own function
and measured that directly: 528, 336, 174 and 84 out of four thousand at levels
13, 26, 20 and 28 — which is one half, one third, one sixth and one twelfth of a
quarter, to three figures. No ambiguity, no flake, and it fails loudly if anyone
ever flattens the curve.

That is three tests in three days that were green while asserting nothing much.
The pattern in all of them is the same: I measured a consequence when I could
have measured the cause.


28 August 2026 — a projection for the mind, and a stale binary for the fifth time
=================================================================================

The Mindcrafter, which is the opposite kind of class to the Monk: no weapon
worth speaking of, no armour worth speaking of, and twelve powers that arrive
purely by being what it is.

Two things needed building rather than importing.

The first was **level bands**. I had assumed a power was one effect chain and
built PLR-02 that way. Zangband's are not. Precognition detects monsters at level
2, finds traps and doors at 5, sees the invisible at 15, maps the level at 20,
grants telepathy from 25 to 39, detects everything at 30, and lights the entire
level at 45 — one power, on one key, for a whole career, becoming something else
underneath you. 4.2's effect chain runs start to finish with no notion of when a
link applies, so powers grew ``power-when``: a group of effects and the levels it
is good for. Eight of the twelve need it. Pleasingly, this is also exactly the
mechanism PLR-01 said was missing for the Draconian's breath changing every five
levels, so a gap I recorded three days ago closed itself as a side effect.

The second was **a projection for psionic force**. Every damaging type Angband
has is an element — you resist it with a flag or with armour. ``GF_PSI`` is not
that. It asks whether there is a mind there to hurt, and where there is none it
does nothing at all, however hard you hit. I could have approximated it with mana
damage and nobody would have filed a bug, but it is the entire character of the
class, so ``PROJ_MON_PSI`` went in properly. The nice part is that 4.2 already
keeps the flags for it: ``EMPTY_MIND`` and ``WEIRD_MIND`` exist for telepathy,
which is the same question asked the other way round — can this thing be
perceived as a mind? Twenty-six monsters carry each. So a golem is immune to a
Mindcrafter and always was, in data written for something else entirely.

And then, for what I am fairly sure is the fifth time in this project: eighteen
test suites reported *Cannot initialize player classes*, I read the error as a
real parse failure in data I had just written, and it was stale binaries. The
unit-test targets are not in ``all``. ``cmake --build build`` does not touch
them. I know this. It is written in an earlier entry on this page. I still lost
several minutes to it, and the tell was there in the message — "undefined
directive" means the parser does not know a keyword, which for data that parses
fine elsewhere in the same tree can only mean two different binaries.

One thing I did catch. The Monk damage test I wrote yesterday was reporting
figures that swung between five and forty thousand run to run, and I had let it
pass because the assertion cleared anyway. The cause was that a dying monster
capped the count at its own hit points, which truncated the martial figure — the
one being measured — and never touched the bare-handed one. Topping the target up
past anything one turn can reach made it exact, and the real ratio is about fifty
to one rather than the eight I had reported. A test can be green and still be
lying about the number it prints.


27 August 2026 — the Monk, and a test that had been lying for a week
=====================================================================

The Monk went in today, which meant building martial arts from nothing: 4.2's
answer for an empty weapon slot is one point of damage a blow with criticals
explicitly skipped, and Zangband's Monk is a character whose weapon *is* itself.
Seventeen techniques on a ladder, two to eight strikes a turn, armour class for
every slot left empty, and all of it withdrawn the moment you put on plate.

I measured the class mapping rather than guessing it, the way PLR-01 did for
races. Six classes exist in both games, and comparing them field by field gave
conversion factors instead of taste — two of which came back suspiciously clean.
Zangband's searching skill maps to 4.2's at exactly 0.62 in five of the six, and
the device *increment* is 1.00 in all six. That is 4.2 having deliberately
rescaled one and left the other untouched, and it is the kind of thing you only
see if you look.

Then the interesting part, which is the part I got wrong.

Building the Monk I noticed it would be a class with no spellbooks, and went to
check what that meant for the racial powers I shipped yesterday. It meant they
did not work. ``calc_mana()`` returns a maximum of zero for any class with an
empty book list, powers read their cost from spell points, and so a Draconian
Warrior could never once breathe. Zangband's own answer turns out to be that a
character short of mana pays in hit points instead, which is both the fix and, I
suspect, why Zangband wrote it that way in the first place.

*Corrected the same day:* I first wrote here that this locked out nine of
fourteen classes. There are ten classes, and only two of them — Warrior and
Monk — have no spellbooks at all. I had assumed the fighting classes did not
cast, and in 4.2 the Rogue, the Ranger, the Paladin and the Blackguard all do.
The defect was real and the fix is right; the number was me not checking a claim
that happened to flatter the size of what I had just fixed.

That was mine and it was recent. The next one was worse.

My Monk test kept segfaulting, and I spent a while convinced the fault was in
the new martial arts code. It was not — it was in the tests I wrote yesterday,
which do this:

.. code-block:: c

   for (r = races; r; r = r->next)
       if (streq(r->name, "Mindflayer")) power = r->powers;
   player->race = r;

The loop never breaks. By the time it exits ``r`` is ``NULL``, so every one of
those tests had been setting the player's race to nothing at all. They passed —
``player_use_power()`` never reads the race — and they left a null pointer behind
for whatever ran next. Nothing ran next until today.

Three more flakes fell out of the same afternoon: two older tests that placed a
monster by walking east from the player until they found a free square, which
fails whenever the character is standing near a wall. They had been failing
roughly one run in five and I had been rerunning them. They now search outward
in rings, which is what they should always have done.

None of that is glamorous. But a test that passes while asserting nothing is
worse than no test, because it occupies the space where a real one would go —
and I wrote four of them in a row without noticing.


26 August 2026 — nine powers, and a keyboard with nothing left on it
====================================================================

Racial powers went in today (PLR-02): nine things a character can do because of
what it is rather than what it studied. The table is Zangband's own, lifted out
of ``tables.c`` in the archived source rather than reinvented — level, mana cost,
governing stat and failure chance, all nine rows. A Vampire drinks blood at level
5 for 10 mana; an Amberite walks the Pattern at 40 for 75. I would not have
guessed those numbers as well as the original did, and there is a unit test now
whose only job is to stop a later edit quietly repricing them.

The mechanism was straightforward. Getting the *data* to parse was four rounds of
being wrong in a row, and three of them were my own tooling. First every
``power*`` directive came back "undefined", which was a stale test binary — the
unit-test targets are not in ``all``, a trap this project has now walked into
enough times that it should probably be written on the wall. Then an effect took
a radius it did not want. Then ``power-dice:$P`` — which I had simply invented.
Angband's dice syntax has no "player level" token; what it has is a named
placeholder bound by a separate ``expr:`` line, which is how class spells scale.
So the race parser grew a ``power-expr`` to match, and a Draconian's breath is
``$B`` bound to ``PLAYER_LEVEL:* 3 / 2``.

Then the part I did not expect. The command needed a key, and I had given it
``N``. In the roguelike keyset ``N`` is run-southeast — one of the eight running
letters — so the keymap swallows it and the command is reachable only from the
Enter menu. Which was also true of ``J``, the quest log I added a few days ago
and never checked. Angband handles this with a second key per command, the
roguelike alternative, and I had left it zero on both.

So: find a free letter. There isn't one. I enumerated every key bound in the
command tables against every keymap in ``pref.prf`` and the intersection is
empty — all fifty-two letters are spoken for in one keyset or the other, and so
is every usable control key. Twenty-odd years of accreted commands have filled
the keyboard exactly. The nine survivors are punctuation, so the roguelike
bindings are ``&`` for a racial power and ``%`` for the quest log. Not mnemonic,
and I do not think there is a version of this that is. It is a real constraint on
how much more this game can grow sideways, and worth knowing about now rather
than the fifth time I add a command.


23 August 2026 — a review, and the save that was already broken
===============================================================

Before starting anything new I ran a review over everything this project has added
to Angband — about 23,000 lines against the ``angband-base`` tag. Thirteen findings
came back. Twelve were the kind you expect. The first one was not, and the reason
it survived this long is the part worth writing down.

**A green test suite was hiding it.** Saving below ground and then loading in a
fresh process decoded the remembered surface as garbage. The mechanism: ``load.c``
keeps the number of ``SQUARE_*`` info planes a chunk was written with in a
file-static, and the only thing that sets it is the *dungeon* block. The
*wilderness* block is written first, and it also decodes a chunk — the surface the
player is holding while they are underground. So on the first load in a process it
ran with a plane count of zero, skipped the info planes entirely, and read terrain
out of the middle of them. Depending on the file that is a wrong map, a heap write
past ``feat_count``, or ``quit("Broken savefile")``.

There *is* a test for this. ``the-map-survives-a-save-from-below`` does an honest
round trip — save, ``cleanup_angband``, ``init_angband``, load — and it passed. It
passed because two earlier tests in the array had already loaded a savefile, and
``cleanup_angband`` does not reset a static in ``load.c``. From anywhere except the
front of the list the test could not fail. I moved it to the front, watched it
fail, fixed the bug, watched it pass, and left it at the front with a comment
saying why the position is load-bearing. Cross-test static leakage will hide the
next bug of this shape too, and the only defence is a test that runs before
anything else has warmed the state up.

The fix threads the plane count through as a parameter instead of leaving it
ambient, and adds version 6 of the wilderness block, which records it. Versions 3
to 5 pass the compile-time ``SQUARE_SIZE``, which is what those files were always
written with — the count simply went unrecorded — so existing saves are repaired
rather than invalidated.

**Making town placement fast without moving anybody's towns.** A profile said 60%
of world generation was ``wild_in_town``, called once per block of a 17×17 window,
for every candidate block in the world, for every town placed — each call walking
the town list and recomputing origins. Replacing that walk with a block index took
the ``cave/wild`` suite from 80 seconds to 37.

The nervous part is that worlds are regenerated from their seed on load. Change how
a town is scored and every existing character's towns move underneath them. Passing
tests would not have told me that, because the tests check that towns are
*plausible*, not that they are in the same place as yesterday. So I compiled both
implementations side by side, had every call compute both answers and abort on any
disagreement, and ran the whole suite. No mismatches. That is the check I would
have skipped if I were in a hurry, and it is the only one that actually answered
the question.

**The bug the review did not find.** While verifying, ``player/inven-wield`` failed
once in a sweep. I assumed I had broken it, stashed everything, and found it failing
5 times in 100 runs on untouched code — so, not mine, and older than the review.
Chasing it turned up something real: ``drop_find_grid`` picks where a dropped object
lands by asking ``square_isfloor``. In Angband floor and object-holding are the same
set of terrain. Here they are not, because a tree and a shallow stream are
``PASSABLE`` and ``OBJECT`` and deliberately not ``FLOOR``. So a character standing
in a wood who dropped something — or whose pack overflowed — had their own square
rejected, and the item turned up a square or two away; and where no floor square was
both in reach and in line of sight, and trees block sight, it was destroyed. The
test was standing in a tree about one run in twenty.

That is the third time a passable non-floor terrain has broken an upstream
assumption that nothing in the wilderness would be walkable and not floor — the
first was trees not lighting walls, which is already in this diary. Worth a sweep of
the remaining ``square_isfloor`` calls sometime, on the assumption there are more.

**What I got wrong.** My first pass at the spellbook fix moved the unreadable-book
test above the theme roll, which reads better and is wrong: it takes books out of
the theme weighting altogether. Backed it out for a guard on the fallback
assignment, which changes nothing except the one case that was broken. And one of
the thirteen findings I rejected — ``prt_daylight`` writing four characters and
returning six is not a bug, it is a fixed-width field so the rest of the status line
does not shift two columns when the sun comes up.

1025/1025 unit tests and 5/5 integration tests pass.

24 August 2026 — the Unicorn, and a test that measured nothing twice
====================================================================

The deer became the Unicorn, which was the owner's call and the right one. I had
noticed the resemblance while building the deer and said so rather than acting on
it, because renaming somebody else's idea is not my decision; the answer came back
"yes, let's do that", and it cost one data record and eight lines of code. That
ratio is the whole argument for building the general thing first. The `BLESSING`
flag did not know about unicorns, and the Unicorn needed no flag of her own.

What makes her blessing greater is that she is `UNIQUE`. Not a second flag —
being unique *is* the difference. There are deer, and there is the Unicorn, and
one of those is not a kind of thing.

The rest of the day was two failures, one mine and one interesting.

**The x86 one.** The deer test killed the whole `game/wild` suite on Linux and on
msys2 with "Suite died: Floating-point exception", while passing here. That is an
integer division by zero: `py_attack()` divides the turn's energy by the number of
blows, and the test suite had never called `calc_bonuses()`, so blows was zero.
x86 traps it and kills the process; this Mac's ARM quietly yields zero. So the
test passed locally for the *same reason* it crashed elsewhere.

And then, fixing it, the second half: with blows computed correctly the energy per
blow became non-zero, and a test character has no energy, so `py_attack()` never
swung at all and the heal never fired. The test had only ever reached the monster
*because* of the bug — zero energy is always enough for a blow that costs nothing.
The green tick had been resting on undefined behaviour from the moment I wrote it.

I also learned that CI runs `alltests` and I had been running `allunittests`. The
difference is the front-end tests. Two different targets, one of which I had never
run, for however many weeks.

**The vacuous one, twice in a day.** The deer test measures the worst of thirty
bounds. To touch the beast it walked the player to a grid beside it — and when no
adjacent grid was free it skipped that touch. Once the Unicorn was also standing
in the level, it skipped between two and thirty of them depending on where the
last bound landed, and on the runs where it skipped all thirty it reported the
sentinel it had initialised the minimum to and passed, having measured nothing.
999 is greater than 5.

The fix was to stop walking round the beast at all: how far it bounds does not
depend on which side the hand came from. But the lesson is the counter. A loop
that can skip every iteration and still assert successfully is not a test, and I
have now written that same shape three times this week — a road test that scanned
too few blocks, a shop test whose shop had no deep end, and this. What they have
in common is a measurement with no floor under it. The counter is one line and it
is the line that turns a silent pass into a failure.

24 August 2026 — a deer, and where a monster comes from
=======================================================

Two small things, one of which turned into a filing question I did not expect to
find interesting.

The deer came out of the ideas file: *"Deers are magical. When you bump into one,
your HP is restored and it jumps away min 5 tiles."* Almost all of the work was
in the two words that were not in the note.

**"Once."** A full heal for nothing is a good thing to find. A full heal for
nothing that can be had again by walking after the beast and touching it a second
time is a character who never buys a potion again, and the note does not say which
it is. So the beast remembers, in the per-monster flag field, which is written to
the savefile with the rest of the monster — meaning reloading does not persuade it
either. I nearly used a field that is not saved, which would have made the
exploit reappear on every load and be very hard to attribute.

**"At least five."** The teleport effect does not take a minimum. It picks the
grid whose distance best *approximates* what you ask, then varies it by up to a
quarter either way, so asking for five lands short of five about half the time. I
asked for ten and measured the worst of thirty bounds: nine grids. This is the
third time in a week that a number I would have written straight into the code
turned out to need measuring first, and the third time the measurement took two
minutes.

The filing question. A ZangbandTK-original monster belongs in neither of the two
bestiary files: ``monster.txt`` is Angband's, and ``monster.zangband.txt`` is
generated by the import tool and carries a "do not hand-edit" line. I could have
put the deer in the first and left a comment. Instead I added a third file, which
felt like ceremony for one deer until I read the comment already sitting above the
loader explaining why the first two are separate — provenance should be obvious
from the file a thing is in. That argument does not get weaker when the third
category has one member in it. It gets weaker if I let the first exception through.

One more thing I did before touching any of it: the request was "did we document
the nightmares in the manual". We had — in the written manual. The in-game help
had a two-line mention buried in the symbol reference, and looking at that
properly, the whole of towns and services had accreted there: five buildings, the
shop quality ladder, the one-house rule, town names, gates. All of it filed under
"symbols you will see on screen". It has its own page now. The lesson is not about
towns; it is that the honest answer to "did we document that" was "yes, in the
place I was thinking of", and the useful answer needed me to go and look at the
other place.

23 August 2026 — the dream, which is the lotus backwards
========================================================

The inn's nightmare has been on the follow-up list since DEC-32 dropped the
Mythos path, and it went in this afternoon largely because building the lotus
yesterday had already built most of it. The lotus takes places off the world map;
the dream puts one on. Same machinery, opposite sign. If I had done these in the
other order the second one would have been the cheap one.

Two decisions in it worth keeping.

**Seen, not visited.** The obvious implementation of "a dream shows you a town"
is to mark the town as known, and Angband's own flags make it easy to mark the
wrong one. This world has two: a block can be *seen*, which puts it on the world
map, and a town can be *visited*, which is what the magetower's destination list
is built from. Marking visited would have made a night's sleep into free passage
to anywhere in the world — the single most valuable thing in the game, for 25
gold, from a building in every town. Seen is the honest one: the dream tells you
where to walk. You still walk.

I would like to say I saw that coming. What actually happened is that I wrote the
distinction down two days ago while documenting the magetower, and it was still
close enough to hand to catch me before I typed it.

**The dream is about something you have met.** Zangband picked from the deepest
part of the bestiary, which is where its sanity blast wanted to point — the horror
you have never seen is the scarier one if the mechanic is "look at an unspeakable
thing and lose your mind". Without that mechanic it reads differently: a dream
about a monster you have never encountered is a table lookup with a name in it. So
this draws from what the character has actually seen, deepest of three draws. It
scales itself for free — a new character has met almost nothing and dreams of
almost nothing — and it means the thing that nearly killed you last week is the
thing that comes back at night, which is what a bad dream is.

The weighting is by the town's law, and that came from asking why every inn in the
world should be the same inn when there is a whole parameter space sitting there
saying how settled a place is. A frontier town gives you nightmares one night in
four and visions one in eleven; a lawful city inverts it. A town below about 155
law has fallen and keeps no services at all, so the genuinely lawless end never
comes up and did not need defending.

One test caught something I would have shipped. With every block cleared to
unseen, the reveal offered the character the town they were *asleep in* — nearest
unfound place, distance zero, technically correct. It cannot happen in play,
because walking into a town marks its block seen, so this is a bug that only
exists under a test's artificial conditions. I fixed it anyway: a dream about the
room you are sleeping in is not a dream, and a function that can only be trusted
when its caller is careful is one I will misuse later.

23 August 2026 — the lotus, and five places to forget
=====================================================

The first thing in this project that is not a port. Everything until now came out
of Zangband's source with a decision attached about whether to keep it; this came
out of a one-line note in the ideas file: *"Lotus Leaves. Eat them and you forget
everything in 5 turns... you feel a little dizzy.. where am i ...."*

Two things about building it were worth the day.

**"Forget everything" is five features, not one.** Angband keeps knowledge in five
unrelated places and there is no switch that covers them: the level map is per-grid
known terrain, the world map is a flag per block plus a visited flag per town, the
monster memory is a lore struct per race, item identification is an aware flag per
kind, and spells are a learned bit per spell plus an order array. Four of the five
already had the function I needed — ``square_forget``, ``wipe_monster_lore``, and
so on — which is the good news; the bad news is that the failure mode of a feature
like this is quietly forgetting to forget one of them, and nothing in play would
tell you which. So the test checks all five explicitly rather than checking that
the function ran.

**The exception matters more than the rule.** My first pass forgot the world map
entirely, which is obviously right and quietly ruinous: the magetower's
destination list is built from the places the player has found, so a character who
has forgotten every place has a blank map, no fast travel, and nothing to walk
towards. Not a setback — a lost save. There is already a requirement that says the
starting village is always known (WLD-12) and a test that enforces it, so the
constraint was sitting there waiting; I just had not connected it to the new thing.
Home stays known, and the nine blocks around it, or the village is a name with no
ground under it.

That the exception is also exactly right for the source material is luck rather
than design. Corwin opens the first novel with no memory and one certainty: that
there is a place called Amber and he is of it. I did not set out to reproduce that.
I set out to stop the item bricking saves, and it turned out the thing that keeps
the game playable is the thing the novel opens on.

**The delay is the feature.** An item that took your memory the moment you ate it
would be an ordinary bad mushroom, one of a dozen. Five turns of "you feel a little
dizzy..." and then "Where am I?" is a mistake you have time to understand and no
time to undo. Mechanically it is a timed effect that does nothing at all — a fuse —
with the whole of the behaviour hanging off the turn it expires.

One other thing fell out. Chasing whether my new test had broken a neighbour, I
found a test that had been failing about one run in five all along: it walks the
view window sideways and asserts the other axis holds still. True — unless the
character started within a margin of the window's edge, which block alignment
decides, in which case the other axis scrolls for the correct reason and the test
calls a working scroll broken. Third time this week a test has been measuring
something that could not move, or asserting something that was only sometimes true.
The pattern I should have learned by now: when a test is flaky, suspect the
assertion before the code.

23 August 2026 — the axis that had nothing left to say
======================================================

WLD-15 asks for buildings scored on the same parameter space as terrain, which in
Zangband meant population, magic and law. Our world map had population and law
and no magic, so I read the requirement as: add the missing fractal, wire up the
scoring, done. A formality.

It was not a formality, and finding out why was the useful part of the day.

I wrote the scoring, guessed thresholds that looked sensible, and measured. A
quarter of every shop in the world came out on the top rung, and the tiers came
out in the wrong order — more arcane shops than expert ones. The order is easy to
explain: everything above the highest cutoff piles into the top bucket, so if the
cutoff is too low the top rung swallows the tail. The quarter needed a real
answer, so I measured the axes themselves at the 479 town blocks in 40 worlds.

Law runs 104 to 254 with a mean of 208. It is not an axis at a town; it is a
constant with a little noise on it. Of course it is — WLD-08a *sites* towns on
law, so by the time there is a town to score, law has already been asked its
question and given its answer. Population survives, because the size bands are
cut from it and villages are real places. And magic is very nearly uniform,
because nothing anywhere selects for it.

So the axis I had thought of as the formality is the only one with anything left
to say, and the two I already had were mostly spent. That is worth writing down
as a general shape: a parameter you have already used to *choose* something
cannot then be used to *vary* it. It has no variance left where you are looking.

The other mistake was smaller and more embarrassing. A tier raises the level the
goods are generated at, and I asserted that this makes a better shop sell better
things. It does not. The level reaches ``apply_magic()``, so it buys better magic
on the item — real, and worth about three times the plusses at the top rung — but
which *kind* of item a shop sells comes out of ``store.txt``, and nothing in that
list depends on level. So an arcane shop's shelves were exactly as deep as a
plain one's. A long word on the sign with nothing behind it, which is the precise
failure WLD-16a exists to avoid.

The test that caught it nearly failed to. It picked "the first shop with any
turnover", which is the general store, whose entire stock is food and torches:
there is no deep end of that table to bias towards, so the measurement was flat
whether the code worked or not. Picking the widest-ranging shop instead — the
alchemist, levels 1 to 40 — made the difference visible immediately. Two days
running now, a test has been weak in the same way: measuring something that could
not move.

Then object value turned out to be a bad proxy too. A deeper potion is not a
dearer potion, so the alchemist's shelves are worth the same at every rung. The
magic shows up only on things that can carry a plus, which the alchemist does not
sell, so that half of the claim is measured across every shop instead of one.
Three metrics before one of them meant what I wanted it to mean.

Ended at 70 / 18 / 8 / 2 per cent, thresholds taken from centiles of the measured
distribution rather than chosen. 113 hand-authored building types replaced by
three records and a score.

23 August 2026 — a road you can see, and a building that was demolished
=======================================================================

Two faults from the same afternoon, and the second one taught me more.

The road first. "The road appears to end at the beach. That was really a long
walk." It did not end; it turned. A road was one grid wide, and a one-grid road
that turns a right angle in the block you happen to be standing in is a single
square of floor at right angles to the way you are going. There is nothing to
see. Roads are three grids wide now with their corners squared off, which is not
a cosmetic change but the difference between a road that reads as a road and one
that reads as a dead end. It cost about three per cent of the world in paving.

The second was services silently missing from towns — the magetower that was
promised and was not there. I chased that number for a long time and every step
was wrong in the same way.

I assumed placement was running out of room, because a service needs a clear lot
off a street and the shops and the ruins take most of them. So I made more lots
available. Then I moved the services earlier, ahead of the ruins. Then I replaced
sixty random guesses with a systematic sweep of every lot. Then I moved them
ahead of the shops too. Each change moved the failure rate — 65 per cent, then
43, then 48, then 8, then 5 with the largest cities at 11 — and I read the
movement as progress. It was not progress. It was noise on twenty-four samples,
and I had been reading a random walk as a trend for four rounds.

What ended it was giving up on the theory and instrumenting the thing I believed:
print a line whenever placement fails to find a lot. It printed nothing. Not
once, in any band. Every service was being built. The ruin pass that runs
afterwards skips a lot that already has a building on it — by asking
``feat_is_shop()``, and a magetower is not a shop. So the generator built the
magetower and then built a ruin on top of it.

Two lines to fix. Zero failures in 2,100 towns afterwards.

The lesson is not "instrument earlier", which I already knew. It is that I never
checked the premise. "Placement is failing" was never measured; it was inferred
from services being absent, and absent has two causes — never built, or built and
destroyed. I spent four rounds optimising the half of the search space the bug
was not in, and the measurements I took along the way were all consistent with
that, because a random walk is consistent with anything.

One more thing worth keeping: the test I wrote to protect the fix passed on the
first seed range I tried while the bug was still live. Twenty-four villages, all
green, on a lucky seed. The version that actually catches it walks every band,
because the village — small, and left to the ruins — is the worst case, not the
great city I had assumed.

21 August 2026 — the DS, and what a 4 MB machine is actually short of
=====================================================================

Parked, and worth writing down properly because it got further than I expected
and then stopped for a reason I did not expect.

It builds, and it runs. Under an emulator the ROM boots, reads its data off the
card, loads all 1013 monsters, makes a character and generates a world. On a
machine with four megabytes. I had assumed the wilderness would be what killed
it, and the wilderness turned out to be the cheapest thing in the game: the
whole world map is 129 by 129 blocks of six bytes each, about 98 KB, because
terrain comes from a seed as you walk rather than being stored.

What the DS is short of is not the world. It is the game. About 1.5 MB of text in
lib/gamedata, parsed into structs and strings — the bestiary being most of it —
leaves only a few hundred kilobytes free. The live surface chunk is what tips it
over, and it is allocated twice, because after building the level the game
allocates the player's *known* map at the same size. Worse, ``cave_new`` takes a
separate allocation per grid for that grid's flags, so a 144x144 surface is
20,736 allocations whose headers cost more than the flags they hold.

So the DS gets a smaller world: 260x260 grids, one town, all thirteen dungeons
still out there. That is three lines in a constants file, applied to this build
only, and the file is on the card — so the numbers came out of bisecting on real
data rather than out of my arithmetic, which was wrong twice on the way.

Then it failed on the actual hardware, with "Unable to access filesystem". That
is DLDI: homebrew needs a driver for the specific card it runs from patched into
the ROM. Which would be a small thing, except that the first ROM I sent to the
DS had already been patched — by the emulator, which rewrites the file in place
and had stamped its own driver into it. A pristine ROM says "Default (No
interface)" and is 902,656 bytes; after melonDS had opened it, the same file said
"melonDS DLDI driver" and was padded to a megabyte. I had staged the test ROM
next to the thing I was treating as the deliverable, so the emulator quietly
edited the deliverable.

The lesson I will actually keep from the day: never flash a ROM an emulator has
opened. The one I will probably have to learn again: I was confident three times
about a machine I cannot run, and each time it was a measurement on the card that
put me right, not more reading of the source.

21 August 2026 — Windows, twice
===============================

The Windows build works, which I did not entirely expect. The mingw cross build,
MSBuild, MSYS2 and Cygwin all pass and have for a while, so shipping a Windows
zip turned out to be packaging rather than porting.

The 64-bit build was more awkward than the 32-bit one for a reason I would not
have guessed: the libpng and zlib that ship in ``src/win/dll`` are 32-bit
binaries. The CMake option says so outright — *"32-bit x86 only"* — so you cannot
simply point the same recipe at an x86_64 toolchain, and Ubuntu has no
cross-built libpng to substitute. It builds on a Windows runner under MSYS2
instead, statically linked, which also means the zip is one executable with
nothing beside it to lose.

Tested under CrossOver, and I spent a while convinced it was broken because I
kept checking for a window before it had finished loading five tilesets. It was
fine. It created a character, generated a world, saved 58 KB and exited cleanly —
which matters more than the window did, because savefile I/O is where 32-bit and
64-bit actually diverge and the savefile code had changed recently.

No ARM build. Windows on ARM emulates x64, and for a game that spends its time in
level generation rather than a render loop, that is enough.


18 to 20 August 2026 — the world fills in
=========================================

Towns, roads and dungeons, in that order, and each one broke something the
previous one had established.

A dozen towns in four sizes went in, and then three faults made every town that
was not home unusable: building lots were all clamped to the starting village's
size, so larger towns came out as empty fields inside their walls; no town was
drawn at all once the window stopped covering home; and about one town in fifty
lost its down staircase to the gate-cutting. All three were the same mistake in
different clothes — code written when there was one town, at one place, at one
size.

Then a town wall I was standing next to was invisible, but only when I stood in
trees. Angband lights a wall if the grid between it and you carries light onto its
face, which quietly assumes anything blocking sight is a wall nobody can stand
in. ZangbandTK's trees are passable *and* block sight, so the grid I was standing
in was judged to be blocking the light. Only the stretches of wall with grass in
front of them lit up, which is exactly what made it baffling to look at.

Thirteen dungeons, each covering a range of depths and ending at the bottom of
it, so going deeper means crossing the world to find one that reaches deeper.
That is the first thing in the game that is Amber's geography rather than
Angband's.


17 to 18 August 2026 — the website, and four failures in a row
==============================================================

Getting the manual published took longer than writing the manual would have.

The docs were Angband's Sphinx project, essentially untouched: the config still
said ``project = "Angband"``, and the theme was ``sphinx-better-theme``,
unmaintained since 2013, propped up with a hand-written template and a hundred
lines of CSS doing by hand what a current theme does by configuration. Swapping
it was the right move and it broke three separate things, none of which I found
until each one failed in turn:

- The pull-request docs workflow, which installed the old theme by name.
- The CMake documentation target, which copies ``docs/_templates`` — a directory
  that stopped existing when the last file in it was deleted, because git does
  not track empty directories.
- ``scripts/pkg_win``, which created a fixed list of documentation
  subdirectories, and the new theme keeps its assets in different ones.

The lesson is dull and worth writing down anyway: several separate things consume
the documentation build, and I only checked the one I was working in.

The publishing had its own comedy. GitHub Pages defaults to serving a branch, and
guessed ``master /docs`` because that is where docs live — which is right for a
repository whose ``docs`` folder holds a finished site and wrong for one whose
``docs`` folder holds Sphinx *sources*. It sat harmlessly until a branch build
ran, then published ``docs/README.md`` rendered by Jekyll as the entire website.

And the release pipeline had never produced a release. Not once. Four reasons
stacked deep enough that fixing any one would not have revealed the next: the
docs job broken as above; a release that required Windows, 3DS and Nintendo DS
builds before it would publish a macOS disk image; a trigger on every push to
master tagging with a ``git describe`` string; and no ``permissions`` block, so
the token could not write the release it had just built.

The macOS signing was its own thing. The build leaves a signature on the
executable alone, which is worse than none — it declares sealed resources that do
not exist, so macOS rejects it outright rather than treating it as unsigned.
Fixing that meant discovering that ``codesign`` refuses any bundle carrying
Finder information, and that the ``SetFile`` call the Makefile has always made
sets exactly that, immediately after signing. They cannot both be had. The Finder
bundle bit has been unnecessary since Finder started reading ``Info.plist``, so it
went.

I also wrote the wrong first-launch instructions and shipped them: right-click →
Open, which is what everyone remembers, and which Apple removed. On current macOS
you have to be refused once and then use *Open Anyway* under Privacy & Security.


16 August 2026 — the wilderness, and one over-correction
========================================================

The biggest piece of work so far, and the one that made me change a rule.

The principle I had written down was: Zangband was built on Angband 2.8.1, so a
great deal of what Zangband *looks* like is simply what 2.8.1 looked like. Its
town is a rectangular grid of shops because that is what 2.8.1's town was. The
walls, the moat and the gates are dressing on a 2.8.1 town. Reproducing that
would not be rebuilding Zangband — it would be undoing twenty-five years of
Angband and calling the result a variant.

Reading my own principle, I then stripped the rock that 4.2's town clearing is
blasted out of, on the grounds that a ring of granite round a town in a field
reads as a wall, and a walled town was the thing I was avoiding. Playing it showed
me why that was wrong inside a couple of minutes. The rock is not decoration: it
is what keeps the wilderness out of the market square, and what stops line of
sight at the edge of town. Without it the town was open to anything that fancied
walking in, and a new character could see half a county from the staircase.

So the rock stays and the roads go through it. The principle was right; the
inference I drew from it was not. The test is whether a thing is a Zangband idea
or 2.8.1 showing through — not whether it happens to look like a wall.

Two requirements were withdrawn during this work, one of them along with the code
it had asked for. Writing requirements from a 2005 game means some of them
describe problems the new architecture does not have.


15 August 2026 — measuring instead of guessing, and giving up on clean-room
===========================================================================

I started with a clean-room approach: derive requirements from Zangband's
documentation and behaviour, then build against those without reading its source.
I dropped it within days, and I think that was right.

The wilderness settled it. Fractal terrain generation, the height/population/law
decision tree, block caching, and road and river routing between towns are around
nine thousand lines in ``wild1.c``, ``wild2.c`` and ``wild3.c`` that took years to
get right. A requirements document cannot carry that, and rediscovering it from a
description would have been expensive and worse. The rule became: port where the
algorithm is the value, reimplement where 4.2's architecture differs. Reading the
source to understand intent is always correct; copying it into a structure it was
not written for is not.

The lethality numbers came from measurement rather than taste, which I am pleased
about. Across the 450 monsters that Zangband 2.7.5 and Angband 2.8.1 have in
common, Zangband's carried a median 0.73× the hit points and 0.50× the armour
class of the release it forked from. So every monster in ZangbandTK carries 73% of
Angband's hit points and 50% of its armour class, and both numbers live in a data
file where anyone can put them back to 100 and play at vanilla lethality. That
one change is most of what makes the game feel different.

The first weeks were content: 389 monsters, 51 artifacts, all 18 ego types, three
weapon mechanics Angband has no equivalent of, and the Ancient and Foul Curse with
its cascade intact. All of it now has to be read again against the Amber goal,
because it was imported before that goal was written down as a filter rather than
a preference. Some of it will not survive. Reskin before deleting, where the
mechanic is sound.
