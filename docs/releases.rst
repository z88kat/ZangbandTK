===========
Release log
===========

.. note::

   **3.1.1 is the first release**, tagged on 18 August 2026 — a macOS disk image
   and a source archive, on the `Releases page`_. It is marked a pre-release, as
   every release will be while the game is early. What follows is the development
   log, grouped by the milestones the work is organised into, newest first, with
   everything done since that tag under *Unreleased*.

   This page covers ZangbandTK only. For Angband's own long history, which this
   game is built on, see :doc:`version`.

.. _Releases page: https://github.com/z88kat/ZangbandTK/releases

Development is tracked by milestone. **M0 to M6 are complete** — which is all of
Phase 2's world work — and **M7 is done but for two classes**, the Warrior-Mage
and the High-Mage, which are defined by their choice of magic realm and so wait
for M9. **M8, mutations and virtues, is complete.** See :doc:`features` for what
that adds up to in the game, and for what the rest will bring.

Version numbers move with the work — patch for a fix, minor for a feature,
bumped in the commit that does it — so a build can be identified from its title
bar. They begin at 3.0.0, continuing Zangband's own line from 2.7.5-pre1 rather
than the Angband 4.2.6 the code sits on.


Unreleased
==========

Pets follow — 3 September 2026
------------------------------

- **3.81.0** — **The borg plays the game, and says so when it breaks.** B0 of
  the borg plan. Nothing in this repository played the game: 112 unit suites
  test rules in isolation and none of them walks a character out of a town,
  into a dungeon and back. ``scripts/borg-smoke`` does, from a seed, and exits
  non-zero on a crash, an abort or a wedge with the seed printed beside it.

  The borg segfaulted on the first turn of every game because its map arrays
  are sized for Angband's 66×198 dungeon and this game's depth 0 is a 144×144
  wilderness surface. They are now sized by a ceiling that the game's own
  largest level is checked against, so growing ``wild:cache-blocks`` produces a
  startup failure rather than memory corruption.

  Two build defects had to be fixed first, both single-target option leaks that
  failed silently: ``ALLOW_BORG`` reached the executable only in the Windows
  branch, so on macOS and Linux no front end could name the borg, and
  ``USE_TEST`` never reached the core library, so guarded code there compiled
  to nothing. A headless birth was needed too — the four existing front-end
  tests all quit before the game starts, so a live character had never existed
  in the test front end.

  Also new because the phase needed them: a decision budget, so a borg waiting
  for a prompt it cannot see fails instead of hanging a CI job with no
  diagnosis; ``borg-notes?``, which produced every diagnosis in the phase; and
  a mid-run savefile round-trip, pulled forward from BRG-19. The borg reaches
  depth 1 in 300 turns and the same seed gives the same run.

- **3.80.1** — **Every class is asked whether its books and its spells
  agree.** The borg reported the Mindcrafter as having zero spells, which
  turned out to be correct and by design — PLR-06's psionics are twelve
  ``power:`` entries and no realm, and ``game/wild`` already asserted it. Next
  door to it was a real gap: the **Monk** casts twelve books of ninety-six
  spells and its spell count was pinned nowhere, because ``player/realm``'s
  counts table names classes one at a time and nobody had added a row. Fixed
  with the row, and with an invariant asked of ``classes`` itself — books and
  spells present together, the total equal to the sum of the books, every book
  one to eight spells of a named realm, and every choosable realm backed by
  books. Falsified by perturbing the loaded data three ways, and it catches an
  empty book that the existing entitlement test passes. No game change.

- **3.80.0** — **Four pets, and one in twenty walks away.** Two decisions from
  the project owner, both about how the game feels to play rather than about
  the numbers. The carry cap drops from 24 to **four**: it was written as a
  safety valve against a case that would never arise, and it becomes a
  deliberate limit, because *"more then that at it becomes unmanageable as the
  screen will be a mess of pets"*. And each pet now has a flat **5% chance, at
  each level change, of leaving you for good** — *"He does a runner. Gone"*,
  *"Looking for a new owner"* — removed from the game, not left standing on the
  level behind you, and not recoverable under persistent levels either. Read
  next to what it replaces: Zangband deleted **every** pet at every staircase,
  which is the project owner's own justification — *"That's fair enough given
  the original zangband was 100%"*. Compounded, a pet lasts a median of
  thirteen or fourteen level changes, and a full stable of four loses one about
  one descent in five.

  The four ways of not coming now read as four messages, and the distinction
  they carry is whether the pet is still yours: one ran off and is gone, the
  other three are standing where you left them. ``pets:leave-chance`` is data
  and 0 turns it off. Recorded as DEC-68, and the pets chapter is rewritten
  with the whole picture in it — following, the four, the 5% and what it
  compounds to, which losses are permanent, and the upkeep table showing the
  third young red dragon costing 93% of a caster's regeneration.

  The rule is off inside ``game/carry`` and switched on by the tests that are
  about it, because a suite that changes levels constantly and asserts exact
  counts cannot also be losing a pet one descent in five. Getting that right
  took a correction that ``scripts/check-flakes`` found and nothing else would
  have: ``a-carry-then-a-save-round-trips`` re-initialises the game in the
  middle of the suite, which re-reads ``lib/gamedata`` and quietly turned the
  rule back on for every test after it. One failure in twenty whole-set passes,
  and about one run in fourteen for the test that actually broke.

- **3.79.2** — **The gate checks the build input lists.** Three consecutive
  pushes went red on Linux CI for the same reason — a new ``lib/gamedata`` file
  missing from the install list, which would have shipped a packaged build
  without it — because ``scripts/check-build-lists`` was a separate script you
  had to remember and ``scripts/check-build`` did not run it. It does now. That
  is the **fourth** pass added to the local gate because CI caught something
  the gate did not ask about, after ``-Werror``, the missing test-frontend
  option and the GCC pass; the answer has been the same every time.

- **3.79.1** — **A charm test stops assuming a charm is certain.** The saving
  throw is ``level > randint1(power * 3)``, so a level 30 creature against a
  power of 200 rolls against ``randint1(600)`` and wins about one time in
  twenty. Four assertions in ``monster/charm`` called the charm once and
  required it to take — a five per cent flake apiece, and the comment on one of
  them claimed power 200 "beats the saving throw essentially always", which the
  arithmetic does not support. Raising the power only makes it rarer, so the
  tests now ask whether a creature *can* be charmed rather than whether it was
  charmed first try. The assertions that a charm is *refused* need no such
  loop: those are certainties. Found by ``check-flakes`` reporting the seed.

- **3.79.0** — **What a permanent stable costs** (PLR-26 phase F; DEC-65).
  Pets following the player raised a question: the mana upkeep had gone from
  being the second limit on a stable to being the only one, and the worry was
  that this made Trump straightforwardly the strongest realm. Measured, that
  worry was **wrong**.

  A level 30 Mage regains 36 spell points per hundred turns and keeps two pets
  free. Two young red dragons cost nothing; the third costs 93 per cent, taking
  the same Mage to **2** per hundred turns. The upkeep is charged on the *sum*
  of pet levels, so for anything worth summoning it is a cliff rather than a
  tax, and a caster with an army of deep pets cannot cast at all.

  What following actually changes is narrower: a caster may keep their free
  allowance permanently — two pets at level 30, four at 48, five for a
  High-Mage. A few standing companions rather than a horde, and everything past
  the allowance now costs *continuously* instead of being rebuilt, which makes
  the large stable a worse proposition than before rather than a better one.

  No change to the upkeep. The figures are printed by ``player/pet-upkeep``, so
  a change to the regeneration formula or to monster levels moves them where
  somebody can see it.

- **3.78.0** — **Told which ones, and by name** (PLR-26 phase E). The player
  is told their pets followed, and every pet that stays behind is named
  individually.

  Two defects in the obvious version of that, both found by the tests.

  The messages were said **where the decision was made**, which is before the
  old level is torn down — so they landed on a screen the player never sees.
  The names are now held and said on arrival, beside whatever else is being
  reported.

  And they said *"It cannot follow you."* ``MDESC_IND_VIS`` gives the
  indefinite form for a monster the player cannot see, and nothing is visible
  during a level transition — so every pet got the same useless line, and since
  the message log collapses identical lines, three animals declined produced
  **one**. Named by race now (``MDESC_SHOW``): a player knows their own animals
  whether or not they can see them. The pet roster and the dismissal prompt had
  the same fault and are fixed with it.

  Nothing is said when the player has no pets, so the line means something when
  it appears.

- **3.77.0** — **The awkward cases** (PLR-26 phase D). Uniques, groups,
  stored levels and the arena.

  **A real bug, found by the test written for it.** The racial counter was
  released when a pet was lifted off the old level — which looks tidier and is
  wrong, because the new level is *generated between the collection and the
  placement*. A carried unique therefore became available to the generator, and
  the player could arrive to find a second copy of their own pet standing
  there. Two Grips, reproducibly. The count is now held through generation and
  handed over at placement.

  **Groups.** A carried pet's old group index is cleared, so it joins a fresh
  group of its own. ``monster_group_assign()`` self-heals a *dangling* index,
  which is why a test that only asked for a valid group passed against a build
  with the clearing removed; the case that is not harmless is a **collision**
  with a real group on the new level, which quietly enlists the pet among
  strangers. The test forces that collision and asks that the pet's group
  contains the pet and nobody else.

  **Stored levels.** With persistent levels the old chunk is kept rather than
  freed, so the collection has to happen before ``cave_store()`` — otherwise
  the stored level holds a monster that is also standing on the new one, and
  coming back up hands the player a second copy.

  **The arena carries nothing in and nothing out**, and that is tested rather
  than merely implemented: an arena that let a stable in would be found by a
  player long before it was found by us. Writing that test also turned up that
  ``arena_gen()`` reads ``health_who`` without checking it, so an arena entered
  without an opponent named segfaults — unreachable in play, since only the
  effect can get there, but worth knowing.

- **3.76.0** — **And they bring what they are carrying** (PLR-26 phase C).
  A held object belongs to a chunk twice over — ``obj->oidx`` is a slot in the
  real object array and the player's knowledge of it sits in the same slot of
  the known array — and ``cave_free()`` deletes anything listed with no grid,
  which is every held object. So a carried pet's belongings are released from
  both of the old chunk's lists and re-listed on the new one at matching
  indices, which is ``monster_carry()``'s idiom run backwards and then
  forwards.

  Falsified both ways: skipping the release crashes on freed memory, skipping
  the re-listing is caught by the integrity checker and loses the object.

  A **mimic** still does not follow, and that is not a gap to be closed. What
  it pretends to be is an object on the floor, with a grid and a place in a
  pile: it belongs to the level. Carrying the monster and leaving the disguise
  gives a creature imitating something that is not there.

  One diagnosis worth keeping. A test reported the carried object *duplicated*
  — two apples where one was given — which is precisely the failure this phase
  risks. It was not: the new level had generated a monster carrying an apple of
  its own, and the test was counting the whole object list rather than the pet's
  inventory. Chasing it down rather than adjusting the assertion is the only way
  to tell those two apart.

- **3.75.0** — **Pets follow you downstairs** (PLR-26 as reversed; DEC-67,
  reversing half of DEC-60). Every pet on the level comes with you, put down
  within five squares of where you arrive, nearest-follower first. Anything
  that will not fit is left behind and **named individually**, however many
  lines that takes — arriving with four when you left with six and not knowing
  which two you lost is worse than losing them.

  Zangband deleted every pet at a level change, and that was its region model
  deciding rather than a design: no pet-carrying code anywhere in its source,
  no mention in its documentation, nothing in nineteen changelog entries about
  pets. The recommendation here was to keep that behaviour, and it lost:
  *"Pets are one element of zangband that people like. Usually people don't
  leave their pets behind."*

  Two constants, both data: ``pets:max-carried`` (24, a safety valve rather
  than a balance limit) and ``pets:carry-radius`` (5, **measured** — a mean of
  6.6 empty grids within three of an arrival against 16.9 within five, and the
  player lands on a staircase, which is usually in a corridor).

  A pet carrying objects is **not** carried yet; it is left behind and told
  why. Its held objects belong to the old chunk's object list, and moving them
  is the next phase. The alternative was destroying a player's property to make
  a transition tidy.

  Nothing follows the player into an arena or out of one.

- **3.74.0** — **The chunk's bookkeeping is checked** (M10 phase A of
  PLR-26's reversal). ``cave_check_integrity()`` and a ``game/integrity``
  suite: object indices agree with the slots the chunk filed them under, held
  objects name the monster holding them and are listed, monsters agree with the
  map about who is standing where, mimicked objects point back, and group
  indices name groups that exist.

  4.2 ships ``object_lists_check_integrity()`` and nothing calls it in tests —
  it is built on ``assert()``, which aborts rather than reporting and says
  nothing about monsters. This one reports, covers both lists, and is safe to
  call from a test.

  ``scripts/check-flakes`` also reports the **seed** of a failing pass now. That
  is the whole diagnosis of a rare flake: without it a one-in-three-hundred
  failure is unreproducible and the harness can only say "intermittent". It
  earned that on the run it was added for, naming three failures in
  ``monster/ally-ai`` that six, forty and two hundred passes had not found —
  all of them the same mistake in the test rather than the code, assuming a
  randomly generated level would leave a free grid where one was wanted.

  It is here ahead of pets following the player downstairs, which moves live
  monsters and their held objects between chunks. That failure mode does not
  crash where the mistake is: it surfaces as a corrupt savefile or a duplicated
  artifact several levels later. An instrument that fails *at* the corruption is
  worth more than care taken while writing the thing that might cause it — and
  these invariants have never been checked at all, so it earns its place either
  way.

Pets — 3 September 2026
-----------------------

- **3.73.0** — **Trump** (M10 phase 7; DEC-64, discharging DEC-54). The
  seventh realm, deferred whole in 3.55.0 because fourteen of its thirty-two
  spells summon a creature that serves you and nothing could. **Every realm now
  has books behind it.**

  Twenty-seven of the thirty-two are built, across four decks — *Conjurings &
  Tricks*, *Deck of Many Things*, *Trumps of Doom*, *Five Aces* — for the six
  classes Zangband entitles: Mage, Priest, Rogue, Ranger, Warrior-Mage,
  High-Mage. The Mage, Priest, Warrior-Mage and High-Mage now carry 28 books
  and 224 spells apiece.

  Zangband writes ``bool pet = success`` in every Trump summon, with a comment
  recording that it had been a dice roll and was deliberately made certain. So a
  Trump summon that goes off is a pet; the angry version is a *failed* casting,
  which reaches the miscast machinery. A test pins zero plain summons in the
  realm, because one mapped the old way would look right everywhere else.

  Five are deferred, each for its own reason and each named in a test so a sixth
  cannot join them quietly. *Shuffle* is the interesting one: a d120 read off
  twenty **unequal** bands, and 4.2's ``RANDOM`` picks uniformly — the outcomes
  could be listed and the weights could not, and the weights are the spell.

  Two things turned up on the way. The converter's book-line pattern assumed
  every realm's books were called "books"; Trump's are a **deck**, so its books
  were invisible and the realm before it in each class appeared to run on
  through them. And ``TransferLib``, which stages the game data for the unit
  tests, compares timestamps and can decline to re-copy a file reverted within
  the same second — two falsifications read a stale copy before that was
  understood. ``scripts/check-flakes`` now stages it every run.

- **3.72.0** — **Where pets come from** (M10 phase 6; PLR-28, PLR-32, DEC-63).
  Three charm projections and a pet-summoning effect, and with them the queue
  this milestone has been holding since M8 comes off.

  Built: Life's *Day of the Dove*, Nature's *Animal Taming*, *Animal
  Friendship* and *Summon Animal*, Death's *Enslave Undead*, the ``GROW_MOLD``
  mutation, and the Wand of Tame Monster. Restored: Chaos's *Summon Demon* to
  one demon in three serving you, which DEC-53 recorded as a loss; the
  ``HYPN_GAZE`` mutation from a timed command to a real charm; and
  ``ATT_DEMON`` to one in six arriving friendly. **Spells shipping without an
  effect fell from 120 to 83.**

  A summon arrives on its summoner's side, which is why the upkeep is charged
  on the sum: a pet that summons builds you a stable you are paying for.

  Two things stayed deferred with their reasons **corrected** rather than
  carried. *Raise the Dead* animates the corpses actually lying on the floor,
  and 4.2 has no corpse objects — "summon undead as pets" would be a different
  spell wearing its name. The magical figurine summons the monster named in it,
  and 4.2 has no object whose name interpolates a monster race; it is re-filed
  with the ten statues, since a figurine is a statue that does something.

  Still open from PLR-28: Mindcrafter domination and Chaos patron gifts.

- **3.71.0** — **Turning on your own** (M10 phase 5; PLR-24, PLR-33, DEC-62).
  Anything the player does that hurts an ally turns it hostile, and carrying
  something that aggravates turns them all. It costs virtue: Individualism up,
  Honour, Justice and Compassion down — Zangband's own four writes, dead numbers
  there and live ones here.

  The anger goes in **one** place. ``mon_take_hit()`` is the player-caused
  damage entry point in 4.2 — melee, missiles and every projection reach it,
  while monster-caused damage goes through ``mon_take_nonplayer_hit()`` — so a
  pet cannot blame the player for something else's fireball. Zangband had to
  write its call at seven sites and missed some.

  **PLR-24 asked for a confirmation prompt; Zangband does something better.**
  Walking into an ally changes places with it. The danger was never that a
  player decides to punch their own animal, it is that the animal steps into the
  doorway on the turn the player was walking through it — and since pets follow
  you, a prompt on that step would appear constantly and be answered unread. The
  exceptions are Zangband's: confused, hallucinating, stunned, berserk, or
  unable to see it.

  Aggravation is checked in the monster's turn rather than in
  ``monster_reduce_sleep()``, where 4.2 reads the flag — no ally is ever asleep
  since 3.68.0, so the rule would have been unreachable for exactly the monsters
  it is about.

- **3.70.0** — **Pets cost mana, and earn you nothing** (M10 phase 4; PLR-30,
  PLR-31, DEC-61). The two halves of pet balance, which Zangband's own
  documentation says are the whole of it.

  A *count* is free — ``1 + level/pet-upkeep-div`` pets, the divider now class
  data at Zangband's three values (20, 15 for the Mage, 12 for the High-Mage;
  the three 4.2 classes take their DEC-55 donor's). Past that count the **sum of
  the pets' levels** is the percentage of mana regeneration withheld, clamped to
  5..95 — for the whole stable, not for the excess. That cliff is the balancing
  pressure: two pets inside the allowance are free and a third is charged for
  all three.

  The weight is the monster's level, and that was measured rather than assumed.
  Zangband's code adds ``hdice * 2`` where its documentation says "the sum of
  the levels"; across its 883 monsters those are equal for 48% exactly, median
  difference zero, within two for 96% — the same number written twice.

  **A Blackguard is exempt, deliberately.** Its mana regeneration is negative,
  so applying the factor would make a stable of pets slow the burn down and pay
  the player for what the mechanism charges for. Measured: 15 spell points burnt
  over a hundred turns alone, 1 with the factor applied. Zangband had no such
  class, so this is our call and DEC-61 records it.

  PLR-31 turned out to be already true and is now proved: monster-versus-monster
  kills award no experience and leave uniques alive, so a pet can neither farm
  nor clear a vault.

  The sanitizer pass found a **real use-after-free** on the way through, of the
  same shape as the four fixed in 3.60.x: the stacked monster-message queue
  holds race pointers, ``cleanup_angband()`` frees the races, and nothing
  emptied the queue — so a message left over from one character was read after
  the races were gone when the next character flushed it. Cleared on cleanup
  now.

- **3.69.0** — **Pets take orders** (M10 phase 3; PLR-25, PLR-26, DEC-60).
  The pet menu on ``A``: five leash lengths, two switches, a roster and a
  dismissal. Zangband used ``p``, which is auto-explore here.

  The requirement calls these "nine command modes with per-mode distance
  behaviour"; they are nine entries of which five are distance modes. All nine
  are built. The orders are one policy for every pet at once, which is how
  Zangband holds them, and they survive a save in player block version 6 — the
  leash written signed, because its sign is its meaning and reading it unsigned
  turns "stay twenty-five squares away" into a leash of 65511.

  **Pets do not follow you downstairs, and that is deliberate.** PLR-26 asks
  for it; Zangband does not do it. There is no pet-carrying code anywhere in
  2.7.5 — the mechanism Hengband later added for this is absent from both
  archived lineages — and its documentation, which covers the upkeep, the
  killing-blow rule and every way of obtaining a pet, never mentions taking one
  with you. A pet is a per-level asset, which is also most of the answer to
  whether a summoner trivialises the game. A test pins it, so reversing it is a
  deliberate act. Levels kept under the persistent-levels option do keep their
  pets, which has been true since 3.67.0.

- **3.68.0** — **Allies fight** (M10 phase 2; PLR-23, DEC-59). A monster on the
  player's side now picks its own enemy, stays awake while it is nowhere near
  the player, casts at what it is fighting, and comes back when it drifts off
  the leash.

  Most of the machinery was already in the tree. 4.2 carries a complete
  monster-versus-monster path built for the Necromancer's command power —
  ``monster_attack_monster()`` for blows, ``mon_take_nonplayer_hit()`` for the
  damage, and monster-aware targeting in the spell code — so phase 2 reuses it
  and writes none of it. That also means **PLR-31 is nearly already true**:
  ``mon_take_nonplayer_hit()`` awards no experience and leaves uniques at one
  hit point, so a pet can neither farm experience nor finish a unique.

  Allies get their own branch of ``get_move()`` rather than a swapped target,
  because everything that function does afterwards is about the player: the
  noise and scent heatmaps flow from the player's grid, the flee logic runs away
  from it, and the pack AI works to surround it.

  One ordering choice is load-bearing: the enemy check comes **before** the
  push-past check, because ``monster_can_kill()`` lets a ``KILL_BODY`` monster
  walk over a weaker one and delete it. Without that, a pet standing between the
  player and something large would vanish with no blows struck.

  The three standing orders pets will obey — leash length, doors, picking things
  up — arrive with the player block at version 6. Older characters load with
  Zangband's defaults: follow me, doors shut, hands off.

- **3.67.1** — **A wilderness test stops depending on the world it drew.**
  ``game/wild``'s ``a-towns-services-are-built`` walked the generated world and
  ended with ``require(checked > 1)`` — a floor on how many towns the *world*
  gave services to, which is a random draw. Measured across thirty worlds it
  ran from two towns to twelve, and a world with one or none is legal, so the
  suite failed about one run in twenty and said nothing about the code when it
  did. Found by ``scripts/check-flakes``, one release after it was written.

  Replaced with two deterministic tests rather than a looser floor.
  ``town_gen_wild()`` takes the service mask as an argument, so the masks can be
  chosen: each of the six services alone, then all six together, then none —
  that last being the case a random world almost never supplies and the only
  one that catches a generator building something it was not asked for.
  Falsified both ways, by making the generator ignore the mask and by making it
  build nothing.

- **3.67.0** — **A monster can be on your side** (M10 phase 1; PLR-22, PLR-27,
  PLR-29, DEC-58). Angband 4.2 has no such notion — searching its source for
  *friendly* finds a comment and a shopkeeper's greeting — so this is a change
  to an invariant the monster subsystem is built on rather than a feature on
  top of it, and it lands on its own before anything depends on it.

  **Three states, not a flag.** Hostile, friendly and pet: a friendly monster
  will not attack you and promises nothing else, where a pet takes orders and
  will cost mana upkeep. Zangband carried this as two bits inside its
  smart-learn bitfield, and neither setter cleared the other, so a monster
  could be both and behaved as whichever test ran first — a released pet went
  on taking orders. One enum field cannot reach that state, and hostile is zero
  so no creation path has to remember anything.

  **Monsters can be enemies of each other.** Alignment is checked before sides,
  which means two of your own pets will fight if one is good and the other
  evil. That needed ``RF_GOOD``, which 4.2 does not have: added, with the seven
  imported races Zangband flags. It also turned up a contradiction — 4.2's
  ``dragon`` base template carries ``EVIL``, so the law drake and the Great
  Wyrm of Law came out good *and* evil, which is Balance rather than Law. The
  balance drake and the Great Wyrm of Balance keep both flags, because Zangband
  gives them both on purpose.

  **Looking at a monster says whose side it is on, first** — before its wounds,
  where Zangband put it last and after the recall prompt. The monster list
  counts pets the way it counts sleepers. The glyph is not recoloured, which is
  also what Zangband did: its pet colours belonged to the Tk client.

  Saved games keep it. The monster record gained a byte, so the ``monsters``
  and ``chunks`` blocks both went to version 2 with version 1 loaders; older
  characters open with every monster hostile, which is what they were. A test
  reads the block headers out of a saved file and insists on the new version,
  because the failure that matters is a forgotten version number rather than a
  lost byte.

  Nothing makes a pet yet. That is phase 6; for now the states are reachable
  through a wizard command added with them.

Balance — 2 September 2026
---------------------------

- **3.66.1** — **A harness for flakes, taking its list from the build rather
  than from disk.** ``scripts/check-flakes -n 12`` runs every suite twelve times
  and reports anything that was not the same every time — the suite, the pass,
  and the individual test, obtained by asking the short suite again with ``-v``.
  A single green run says the tests passed once; it does not say they are
  deterministic, and ``game/wild``'s ``a-refused-power-is-free`` failed one run
  in sixteen for two releases while every gate stayed green.

  The list of suites comes from ``ninja -t commands allunittests``. The
  throwaway script this replaces globbed ``build-*/unittests/*/*/*``, which
  picked up a diagnostic suite that had been written, run and deleted — its
  binary outlived its build rule, and its three tests were counted into two
  totals reported here. Anything on disk the build does not know about is now
  named as an orphan and left out of the count. Current figure: **1246 tests**,
  and twelve consecutive whole-suite passes clean, 14,952 test runs.

- **3.66.0** — **Every entitlement Zangband gives is carried over** (CNT-10,
  PLR-08, DEC-57). Zangband's ``realm_choices1/2[]`` were transcribed verbatim
  from the start — a test has asserted that since 3.53.0 and never failed — but
  the *books* for a realm only ever reached classes that already carried that
  kind of book. So five classes could choose realms with nothing behind them: a
  **Mage** was allowed six realms and had three, a **Priest** likewise, and the
  **Rogue**, **Ranger** and **Paladin** were each short of Death or Arcane.

  Ten class-realm pairs, forty books, **305 spells**. It was an artefact of the
  rollout rather than a decision — the three new realms were *appended* to their
  entitled classes and the four mapped ones *replaced* in place, and replacement
  only touches a class that already has those books. The tell was the asymmetry:
  the Warrior-Mage and High-Mage, emitted after all six realms existed, had
  twenty-four books, and the Mage with the same entitlement had twelve. The
  project owner chose fidelity to the tables.

  **A Mage that can study Death is not Angband's Mage.** Healing at one end and
  *Hellfire* at the other; a Priest may take Arcane. It is a larger change to
  how the game plays than any single realm import, taken for fidelity rather
  than for balance, and BAL-14's dials are the answer if playtest disagrees.

  The new books go on the **end** of each class's list, so every spell index that
  already existed still means what it meant — the Mage's Chaos books are still at
  flat index 64. The savefile fingerprint covers the whole list, so it changes: a
  saved caster **with spells recorded is refused** rather than silently rebound,
  and one with none loads unaffected. The corpus needed no change, because all
  thirty-five files already fail a step earlier on the prayer-book kind DEC-50
  deleted.

  Three things checked and found not to move: the chosen-versus-random gate is
  keyed on the class and never on the realm, so a Priest gaining Arcane still
  learns at random while a Mage gaining Life *chooses* its prayers; no deferral
  is class-specific, so widening who carries a realm widened the deferrals with
  it, 87 to 120; and seven spells sit above level 50 on the Rogue's and Ranger's
  own figures, which is ``usable()`` doing Zangband's arithmetic.

  **One consequence beyond the spell lists.** A Mage's pack now sorts a nature
  book and a prayer book as *readable*, because it can read them — and since
  readable books come first and then sort by decreasing tval, the magic book
  comes last of the three. ``player/calc-inventory`` caught it; the new order was
  derived from the rule before the test was re-run.

- **3.65.1** — **A power paid for in blood is no longer also penalised for it**
  (DEC-56). Racial and mutation powers charged 5% failure per point of mana the
  character was short — but the condition that triggered it, ``csp < cost``, is
  character for character the same condition that decides the price comes out
  of *hit points*. So the surcharge fell on exactly the people already paying
  blood, and on nobody else. They were charged twice for one shortfall.

  A Draconian's breath, cost 25, at the level it arrives: **95% to fail on an
  empty pool, now 7%** — the same as with a full one. Worth saying that the old
  number was not a permanent 95: the level term ate into it, so the power came
  good around level 30. An earlier note of mine claimed a Warrior "still cannot
  breathe", which was that one figure generalised too far.

  **Spells keep their penalty**, and that is not an inconsistency. A spell is
  not paid for in blood; it is paid for in unconsciousness — ``over_exert()``
  faints the caster and may take a point of constitution, after a confirmation
  prompt. Blood is a price, fainting is a risk, and neither needs a surcharge
  on top.

  The blood price itself is unchanged: 13 to 25 hit points for a cost-25 power,
  and still refused outright below that. The two classes with no mana at all —
  the Warrior and the Mindcrafter — are the ones this was blocking.


M9: magic realms — 2 September 2026
-----------------------------------

- **3.65.0** — **The Monk casts, and a gate the field names lie about**
  (PLR-03, PLR-08, DEC-36). M7 built the Monk and deferred its casting: realm
  selection did not exist yet. It does, so the Monk now takes one of **Life,
  Nature or Death** — twelve books, ninety-six spells, on its own row of
  ``magic_info[]``, so DEC-55's donor rule never touches it. DEC-36's prose said
  "choosing between Life and Nature" and was one realm short;
  ``realm_choices1[]`` includes Death, and ``realm_choices2[]`` gives the Monk
  nothing, so it picks one of three and has no second slot.

  **Punching and casting coexist, and they meet in the armour.** A Monk is the
  only class that is both ``MARTIAL_ARTS`` and a caster, and Zangband weighs the
  *same six slots* twice: at ``100 + level * 4`` for the martial penalty and at
  ``spell_weight`` for the mana one. The Monk's ``spell_weight`` is **300 — the
  Mage's figure, not the Priest's 350** — so a Monk in heavy armour loses its
  unarmed bonuses *and* its spell points together. Both halves are asserted,
  because a class where one quietly disables the other is the failure worth
  testing for.

  **A Monk learns its spells at random, like a Priest** — and that is the thing
  no field name advertises. Zangband has a ``spell_type`` field that looks
  exactly like the chosen-versus-random switch and **nothing reads it**; the real
  gate is the class's own ``spell_book`` constant, which stays ``TV_LIFE_BOOK``
  whichever realm the Monk picks. So the Monk does *not* get ``CHOOSE_SPELLS``,
  which is the opposite of what treating it as "just another caster" would have
  done. Applied to the six classes both games share, the rule agrees with
  Angband's own convention six times out of six.

  **It also found a defect in a class shipped six versions ago.** The
  Chaos-Warrior's constant is ``TV_SORCERY_BOOK``, so it chooses its spells, and
  3.58.0 gave it books without the flag — it has been learning Chaos at random
  ever since. Fixed here.

  Three things checked and found not to apply: Zangband's glove penalty is gated
  on ``TV_SORCERY_BOOK`` so a Monk never had one (and 4.2 has none at all); none
  of the twenty-nine ``CLASS_MONK`` sites touches mana or failure; and Zangband's
  own study command filters books by the class constant, which means a Zangband
  Monk of Nature cannot select its own books — a bug in the original, not carried
  across, because books here are matched by realm.

  **Documentation and plan corrections** from the 2 September audit: four doc
  claims that had gone stale (three realms described as still holding Angband's
  spells; a chapter note contradicting its own chapter; a mutation counted as
  broken that works), two plan lines still reading as open decisions after they
  were settled, CNT-10 phrased honestly as met for six realms of seven with Trump
  deferred rather than as a tick, and the **mana-shortfall penalty** written into
  the plan as the one thing outstanding for the project owner — it had been
  recorded only in the options chapter since 3.49.3, which is why it kept being
  reported as "nothing outstanding".

- **3.64.1** — **The Windows CI failure was a missing directory, and the gate
  now builds the way that job builds.** ``game/saves`` failed two Windows CI
  runs, 0/1, while Linux and macOS were green — and it was neither Windows nor
  the sanitizers.

  The savefile corpus is staged into the unit tests' working directory, and that
  staging hung off ``SUPPORT_TEST_FRONTEND`` — an option about the *end-to-end*
  test front end, which has nothing to do with whether the unit tests can find
  their data. It defaults to OFF; ``linux.yaml`` passes it and ``msys2.yaml``
  does not, in all three of its jobs that run the tests. So the suite was
  looking for ``tests/saves`` in a directory that did not exist, and failing on
  the assertion written for exactly that ("absent rather than empty is a real
  answer"). Staging now hangs off ``allunittests``, so any configuration that
  can run the unit tests can feed them.

  **Direction of the failure: neither.** No save loaded that should not have,
  and none failed that should not have. Reproduced on macOS by configuring as
  that job does, and confirmed by staging the corpus into that same build, where
  the whole suite passes **1242/1242 under ASAN+UBSAN with no sanitizer reports
  at all**.

  **The corpus is now one test per savefile** — 38 rather than 1. A tally of
  "0/1" was why two CI runs could not say which file had broken or in which
  direction; ``game/saves 37/38`` names it, and an absent corpus says so in
  words and reports the path it looked in. Two invariants came out of writing
  it: a manifest entry naming a file that is not in the corpus used to pass
  silently, and nothing checked that every file had actually been offered to a
  test.

  ``scripts/check-build`` gained a sixth pass that reproduces that CI job —
  Debug, ASAN+UBSAN, and *no* ``-DSUPPORT_*`` options at all. The missing
  options are the pass, not an oversight; the sanitizers are worth having on
  their own but would not have caught this. And ``.gitattributes`` now pins the
  corpus as binary, which git's heuristic already got right, because the corpus
  is the one place a silent byte-for-byte change would go unnoticed until a
  character failed to load.

- **3.64.0** — **The Warrior-Mage and the High-Mage, and PLR-03 is closed**
  (PLR-03, DEC-36). M7 built three of Zangband's five extra classes and deferred
  these two, because each is *defined* by which realms it may choose and realm
  choice did not exist yet. Every class in PLR-03 is now playable.

  A **Warrior-Mage** studies Arcane in its first slot, always, and anything it
  likes in the second — the generalist realm plus one specialism, on a class
  that fights respectably and levels slowly because it pays for both halves. A
  **High-Mage** chooses one realm out of seven and gets no second slot at all,
  and Zangband's table pays it for that: it reaches a realm's deepest spells
  earlier and for less mana than any other caster, on the worst hit dice in the
  game. That difference lives entirely in two ``realm-choice:`` lines, so that
  is what ``player/realm`` asserts, both directions.

  Twenty-four books apiece, over the six realms that have content. Both are
  entitled to Trump and neither is offered it, because it has no books (DEC-54).

  **The skill conversion is DEC-36's, re-run rather than re-judged.** That
  mapping was measured against the six classes both games share and recorded as
  a table of rules — save increment × 0.42, search base × 0.62, melee × 3.1 and
  its increment × 0.56, shoot × 3.45 — and applying it here reproduces the
  Monk, the Mindcrafter and the Chaos-Warrior exactly, field for field, which is
  what made it safe to apply to two classes nobody had converted.

- **3.63.0** — **Death is Zangband's, and DEC-50 is finished** (CNT-10, DEC-50,
  DEC-55). Thirty-two spells in four books — Black Prayers, Black Mass, Black
  Channels, the Necronomicon — replacing the five shadow books the Necromancer
  carried and the three the Blackguard had. **All four of the realms Angband
  already had now hold Zangband's spells**, and every realm in the game is four
  books of eight, which is Zangband's shape throughout.

  Both of Death's classes cast on borrowed figures, because Zangband had
  neither: the Necromancer on Zangband's Mage, the Blackguard on its Paladin
  (DEC-55).

  **A miscast Death spell hurts**, which 3.59.0 left out on purpose — it changes
  the balance of a class whose content had not been replaced yet, and now it
  has. ``(book + 2)d6`` on a roll against the spell's depth in the realm, plus a
  one-in-six chance of losing experience from the realm's second half unless you
  have Hold Life. Out of the **Necronomicon** it is worse half the time: a
  saving throw against confusion and hallucination, and failing the next one
  instead costs a point of intelligence *and* a point of wisdom. It is the only
  spell failure in the game that can permanently cost a statistic.

  Four deferred: *Enslave Undead* and *Raise the Dead* need a creature able to
  take your side; *Wraithform* needs an incorporeal player, which this game has
  no notion of; and *Omnicide* is a whole-level sweep with a running mana cost
  that would otherwise duplicate the realm's own *Mass Genocide*.

  ``GF_HELL_FIRE`` is the last of DEC-53's missing elements to matter. Zangband's
  hell fire does double damage to *good* monsters, and 4.2 has a projection that
  favours evil and none that favours good, so *Malediction* and *Hellfire* use
  ``MANA``: the property that decides how they play is that nothing resists
  them. ``NETHER`` was the closer flavour and the worse spell — undead resist
  nether, and it would have handed the realm's own quarry immunity to its
  biggest attacks.

- **3.62.0** — **Nature is Zangband's, and the Druid casts on borrowed figures**
  (CNT-10, DEC-50, DEC-55). Thirty-two spells in four books — Call of the Wild,
  Nature Mastery, Nature's Gifts, Nature's Wrath — replacing the five nature
  books the Druid carried and the two the Ranger borrowed from it. The Ranger
  now gets the whole realm and goes from ten books to twelve.

  **The Druid is the first class to cast on another class's figures.** Zangband
  never had a Druid, so ``magic_info[]`` has no row for it, and DEC-50 replaces
  its realm anyway. **DEC-55** settles that: a class Zangband never had takes
  the figures of the one it matches on ``spell_first``, ``spell_weight`` and
  casting stat — the three constants that six of Angband's nine casting classes
  match *exactly and by name*, which is what makes the key evidence rather than
  a guess. The Druid's are 1 / 350 / WIS, which among the six Zangband classes
  carrying Nature is the Priest's and only the Priest's.

  Recorded as an obstacle "wanting deciding"; it turned out to be a derivation.
  The same rule gives the Necromancer Zangband's Mage and the Blackguard its
  Paladin, which Death will use.

  **Five of the thirty-two are deferred, and three are one wall**: *Animal
  Taming*, *Summon Animal* and *Animal Friendship* all need a creature able to
  take your side, which is what deferred Trump whole. The other two are *Stone
  Tell*, whole-object identification in a game that replaced it with runes, and
  *Protect from Corrosion*, which is an object property here rather than
  anything an effect reaches.

  Two tool faults surfaced. A realm's book region can hold a directive that is
  not about books — the Druid's ``equip:nature book:[Lesser Charms]`` sits
  between its first book's properties and that book's first spell, and cannot be
  moved out because an ``equip:`` line naming a book must follow the ``book:``
  directive that creates the object kind. Replacing the region wholesale deleted
  it, and the game then refused to start. And ``extract_realm_block`` ran to the
  end of the class block, swallowing the banner comment that separates one class
  from the next and moving the Priest's heading into the middle of the Druid's
  spells.

- **3.61.0** — **Arcane is Zangband's** (CNT-10, DEC-50). Thirty-two spells in
  four books — Cantrips for Beginners, Minor Arcana, Major Arcana, the Manual of
  Mastery — replacing the five magic books the Mage and the Rogue carried. The
  second of the four realms Angband already had, after Life.

  **All four are sold in town**, for 100, 250, 1000 and 2500 gold, which is the
  realm's whole bargain and the half of the spoiler's sentence that pays for the
  other half: Arcane "has no ultra-powerful high level spells" *and* "all Arcane
  spellbooks can be bought in town". Emitted under the generic two-town rule,
  half the realm was unbuyable and the reason to choose it was quietly gone. The
  flag is not cosmetic — ``store.c`` stocks every town book permanently and
  never a dungeon one.

  **A Mage no longer learns its attacks from Arcane.** One ball, one bolt and
  one beam in thirty-two spells. That is Zangband's design, where a Mage takes
  two realms and the second is where the damage lives; Arcane plus Chaos is the
  classic pair. DEC-49's consequence is therefore inverted, and its addendum
  says so rather than being edited to match.

  Two deferred: *Phlogiston*, because refuelling a light is a command in 4.2 and
  not an effect, and *Detect Enchantment*, for the same reason Sorcery's is.

  Along the way, ``apply_realm.py`` — the tool that swaps a realm's books rather
  than appending them — learned to fix starting equipment. A class can begin the
  game holding a book by name, and the Rogue's ``equip:magic book:[First
  Spells]`` outlived the book: the game refused to start with "unrecognized
  sval". Nature and Death will each hit the same thing.

- **3.60.2** — **The gate checks the realm content, and a flake is fixed.**
  ``scripts/check-build`` gained a fifth pass: ``zconv realms --check`` asks
  whether every spell name in ``realmmap.toml`` matches Zangband's own table, in
  both directions, and whether every realm block in ``class.txt`` is still what
  the converter produces for that class. This is the guard against the failure
  that put Sorcery's *Teleport* into the game doing nothing — a name keyed
  wrongly yields a spell with a level, a mana cost, a failure rate and no
  effect, which parses, appears in the book, casts, and does nothing. With three
  realms and about 128 more spells still to import, noticing is not a plan.
  Proved by reintroducing the mis-key (caught) and by hand-editing one spell's
  experience value in ``class.txt`` (caught).

  The pass picks its interpreter rather than assuming one: the converter reads
  TOML with ``tomllib``, which is Python 3.11 and later, and ``python3`` on the
  development machine is 3.9 — so taking whatever is on ``PATH`` made the pass
  die on an import and say nothing about the realms. A missing interpreter is a
  failure with a recipe, not a skip.

  **And ``game/wild``'s ``a-refused-power-is-free`` was failing about one seed in
  sixteen**, on master before this work. It hand-set a spell-point pool and
  asserted an upper bound on what a power charged. Neither holds: the power's
  effect projects, ``project()`` recalculates when anything is pending, and a
  mind blast aimed nowhere in particular can come back on the caster — psi
  damage drains experience, which drops the level and shrinks both pools. On the
  failing seed the character had 43 spell points before a 12-point power and 2
  after. The test now takes the pool the game computes, checks it can hold the
  price, and asserts only the lower bound, which survives because nothing gives
  mana back. The Draconian test beside it had already reached that conclusion
  for the same reason.

- **3.60.1** — **The savefile fence is live again, and three latent faults are
  fixed.** DEC-50's Life realm left every one of the thirty-five historical
  savefiles refused, so nothing proved a character could be saved and loaded at
  all — and the spell-list fingerprint, which is exactly the guard the three
  remaining realm replacements need, had stopped being exercised by anything:
  every corpus file now fails a step earlier on a missing object kind, so the
  fingerprint could have been broken outright with the suite still green.

  ``game/roundtrip`` builds its own characters instead. A Priest with realms
  chosen and prayers learned survives a save and load with the same spells at
  the same indices; the fingerprint **refuses** that character when a spell in
  the class list has been renamed underneath it, and **accepts** it when it has
  not. The rename is deliberate — it leaves ``total_spells`` alone, so the count
  check cannot catch it and only the fingerprint can.

  Making that possible needed three fixes, all the same defect — memory freed
  and left live — and all invisible while a process only ever starts the game
  once:

  - ``cleanup_angband()`` freed the data directory strings without clearing
    them, so the next ``init_file_paths()`` double-freed and libmalloc aborted.
  - ``vformat_kill()`` freed the ``format()`` buffer and left it live, so the
    next ``format()`` wrote into freed memory and the abort landed on some
    unrelated ``free()`` much later.
  - ``calc_spells()`` read the spell arrays before they existed. Birth chooses a
    class and only initialises the arrays at the end, so between the two a
    literate class has none — and every class change recalculates bonuses.
    Upstream survived it by reading freed memory; clearing those pointers in
    3.58.0 turned that into an honest NULL dereference.

  Together they are why a unit test could not build a second character in one
  process, which was recorded in 3.60.0 as its own piece of work.

- **3.60.0** — **Life is Zangband's realm now** (CNT-10, DEC-50). Thirty-two
  prayers in four books — *Book of Common Prayer*, *High Mass*, *Book of the
  Unicorn*, *Blessings of the Grail* — replacing the five Angband prayer books
  the Priest and the Paladin carried. The first *replacement* rather than an
  addition, and the first realm where 4.2 already had content.

  Twelve of the thirty-two chains came from spells 4.2 already had, because
  Life *is* 4.2's `divine` under DEC-49. Two are deferred: *Day of the Dove*
  charms, which needs monster allegiance and waits for M10 with Trump, and 4.2
  has no effect that blesses a weapon. Two of 4.2's own shortfalls are recorded
  rather than hidden — there is no `DISP_DEMON` projection, so *Exorcism* and
  *Dispel Undead & Demons* dispel undead by name and demons by being evil,
  which is wider than asked; and `GLYPH` places one glyph where *Warding True*
  laid a ring.

  The Paladin gains the most: 4.2 gave it three of the Priest's books and
  sixteen prayers, and Zangband's table gives it all thirty-two, at much higher
  levels.

  **Every savefile in the corpus is now refused, and that is the accepted
  cost.** A book's object kind is created by its `book:` line, and a savefile
  names objects by tval and sval *as text* — so deleting `[Novice's Handbook]`
  made every save holding one unreadable, and every save holds one, because
  every save carries the town temple's stock. Measured before the change: the
  two town prayer books appear in 35 of 35 files, the three dungeon books in 0,
  1 and 1. The choice between Zangband's titles and keeping the two town ones
  was put to the project owner with those numbers; Zangband's titles were
  chosen. The thirty-five files are listed in ``tests/saves/EXPECTED-FAILURES``
  with the reason, and the suite still fails if one of them *loads* or if
  anything crashes rather than refusing. **It no longer proves a character can
  be saved and loaded at all** — a fresh corpus of played characters is needed
  for that, and is the next thing this suite wants.

M9: magic realms — 1 September 2026
-----------------------------------

- **3.59.0** — **Chaos spells backfire** (CNT-10). A failed Chaos spell may
  produce a chaotic effect instead of merely failing, and how bad it is depends
  on how deep the spell was: *Magic Missile* never backfires, *Call the Void*
  almost always does. Zangband's forty-band table, boundary for boundary, from a
  short teleport at the top to the Ancient and Foul Curse at the bottom. Only
  three bands are substituted — the six *bizarre* summon groups and the
  Cyberdemon do not exist here, and ``wall_breaker()`` becomes a ``KILL_WALL``
  sphere. Everything else was already an effect the game had, ``GAIN_MUTATION``
  from M8 and ``ANCIENT_CURSE`` from M3 among them.

  The scaling is the part that could have been quietly wrong. A spell's index
  counts across its whole class, so Chaos's first spell is 0 for a
  Chaos-Warrior and 62 for a Mage — scaling by that would have looked correct
  for the one class and made every Chaos spell a Mage owns backfire as though
  it were the deepest in the game. ``player/realm`` walks all 218 spells of
  three classes asserting each realm restarts at zero.

  Death's miscast penalty is deliberately *not* built with this: it would change
  an existing class's balance before that class's spell content is replaced, and
  belongs in the Death realm's own commit. See DEC-53's addendum.

- **3.58.0** — **The Chaos-Warrior casts** (PLR-03, CNT-10). Zangband is blunt
  about the class — "trained in Chaos magic. They are not interested in any
  other form of magic. They can learn every Chaos spell" — but until the Chaos
  realm existed there was nothing to give it, so it shipped declaring itself
  unable to hold spell points. It now has all four Chaos books and all
  thirty-two spells from level 2, and it is the only class in the game with
  exactly one realm and no choice about it. Its figures are its own out of
  ``magic_info[]``, not the Mage's.

  The savefile corpus is unchanged at 31 loaded and 4 refused: the version-2
  spell block only checks its fingerprint when the character actually has
  spells recorded, and the Chaos-Warrior in the corpus has none.

  **Two latent faults surfaced doing it.** Swapping ``player->class`` in place
  without re-sizing the spell arrays segfaulted as soon as the class had spells
  — a patron test had been doing exactly that harmlessly for as long as the
  Chaos-Warrior had no books. And ``player_spells_free()`` freed without
  clearing, so a second free aborted; upstream leaves those pointers dangling
  and it is harmless only while nothing frees twice.

- **3.57.0** — **Swap position and Sterilize work** (PLR-16). The two mutation
  powers M8 deferred with the note "deferred to M9, not open-ended", built where
  they said they would be. Eleven mutations did nothing; nine do now.

  *Swap position* had to be an exchange rather than a move. 4.2's nearest
  existing effect brings the monster to you and leaves you where you are, and
  ``player_place()`` asserts the square it puts you in is empty — so the naive
  version does not play wrong, it aborts. ``monster_swap()`` is 4.2's own
  primitive, run on every monster turn, and already handles one of the two
  grids holding the player. A monster that resists teleportation refuses
  outright, which is Zangband's behaviour and is *not* how the same flag works
  against teleport-other.

  *Sterilize* pushes the level's breeder count past the ceiling that gates
  breeding, which is what Zangband does and is temporary in both: breeding
  resumes once enough breeders die. The seventeen-to-thirty-four hit points it
  costs are a separate effect in the chain, so an effect that stops breeding is
  only that — which needed a ``power-effect-msg`` directive the power parsers
  had never grown, since without it a power that hurts its own user could only
  report "yourself".

- **3.56.1** — **A realm you can choose is a realm you can read** (PLR-08,
  CNT-10). Zangband's entitlement table says a Mage may take Trump, and it is
  imported whole because it is the record of what Zangband permits. But this
  game gains a realm's content one commit at a time, and the birth step was
  offering realms with nothing in them: choose one and the character is made,
  the realm is fixed, and the spell menu is empty for the rest of that
  character's life with no way back. It was live for Sorcery between 3.54.2 and
  3.55.0. The birth step now skips a realm the class has no book in.

  **Trump is deferred whole** (DEC-54). Fifteen of its thirty-two spells are
  summons, and in Zangband every one of them turns on whether the creature that
  arrives is *yours*. 4.2 has no monster allegiance at all, so all fifteen would
  collapse onto the failure branch — *Trump Cyberdemon* would spend 60 mana to
  put a hostile Cyberdemon next to you. That is the spell inverted, not a weaker
  version of it, so the realm waits for pets rather than shipping half a book.

- **3.56.0** — **Chaos is playable, and a spell that did nothing is fixed**
  (CNT-10). Thirty-two more workings in four more books — the Sign of Chaos,
  Chaos Mastery, Chaos Channels and the Armageddon Tome — for the three classes
  entitled to the realm: Mage, Priest and Ranger. The Rogue is not entitled and
  did not grow, which is the entitlement table doing its job.

  **Nothing was deferred, which is not the same as nothing changed.** 4.2 has no
  disintegration, no rocket and no radiation, and a spell's blast radius is a
  fixed number here rather than one that grows with the caster. Six spells do
  something measurably different as a result, each recorded next to its own
  entry; the two worth knowing are that *Call the Void* no longer punishes you
  for casting it beside a wall, and that *Summon Demon* always calls a hostile
  one. See DEC-53.

  **A found match rather than a translation:** 4.2's ``WONDER`` effect is
  Zangband's *Wonder* table band for band, down to the ``plev / 5`` added to the
  roll and the comment explaining it.

  **Sorcery's *Teleport* did nothing in 3.55.0 and now works.** Its effect chain
  was keyed under the wrong name, so the converter emitted the spell with its
  level, mana and failure and no effect at all — indistinguishable from a
  deliberate deferral. Two things now catch that: ``zconv realms`` fails if any
  name in ``realmmap.toml`` does not match the spell table, in either direction,
  and ``player/realm`` asserts that every effectless spell carries the deferral
  wording *and* that there are exactly sixteen of them.

  **And a failing test now fails the build.** The CMake runner set its exit code
  only when a suite died or its output was malformed, so a suite reporting
  10/11 printed in red and exited 0 — and ``scripts/check-build``, ``make
  alltests`` and CI all called that a pass. Found by a real failure sailing
  through the gate during this work. Upstream Angband has the same hole.

- **3.55.0** — **Sorcery is playable** (CNT-10). Thirty-two workings in four
  books — the Beginner's Handbook, the Master Sorcerer's Handbook, Pattern
  Sorcery and the Grimoire of Power — emitted by ``zconv realms`` into the four
  classes entitled to the realm: Mage, Priest, Rogue and Ranger. The numbers
  come from ``magic_info[]`` per class, the effect chains from a new
  ``realmmap.toml``, and the two are kept apart on purpose so the Mage's Sorcery
  and the Rogue's are the same spells at different levels rather than two
  transcriptions that can drift. All four classes' blocks reproduce from the
  converter.

  **The books are appended, never inserted.** A Mage's five Arcane books are
  still books one to five with Sorcery's as six to nine, so every spell index a
  saved character holds still means what it meant — and the corpus is unchanged
  at 31 loaded, 4 refused. Inserting would have shifted them, the game would
  have loaded, and the sheet would have looked reasonable.

  **Four of the thirty-two do nothing and say so**: *Identify True*, *Detect
  Enchantment*, *Self Knowledge* and *Explosive Rune*, each needing a mechanism
  4.2 removed or never had. They are in their books with their levels and costs,
  described as beyond what the game can express. A book with a hole in it and a
  note saying so is honest; a spell absent without explanation is not.

  This is also the realm filter's first real test. Until now no class carried
  books from two realms, so removing the filter failed nothing; a Priest of Life
  can now read prayer books and not Sorcery ones, and a Priest of Sorcery the
  other way round.

- **3.54.2** — **Realm choice gates which books you can open**, which is what
  makes the choice mean anything. A class carries the books of every realm it may
  study — a spell lives in a book which lives in a class, and there is nowhere
  else to put one — so the choice made at birth is what decides which of them
  the character can actually read.

  The gate asks how many realms the class has books **in**, not how many it is
  entitled to, and that distinction was a live regression rather than a nicety:
  a Priest may study Life or Death and every Priest book in the game is Life, so
  filtering on the entitlement left a Priest who chose Death unable to read
  anything at all. The gate is therefore inert until a class carries books from
  two realms, which is exactly when Sorcery arrives.

  Honest about what the tests cover: a Priest who chose Death is proven to still
  read their prayer books, and gating on the entitlement instead fails that. But
  *removing* the filter fails nothing yet, because no class in the data carries
  two realms' books for it to sort. That test comes with Sorcery's content.

- **3.54.1** — **FETCH, built once for three callers**, and telekinesis works.
  Zangband's ``fetch()`` is called by the telekinesis mutation, by Sorcery's
  *Telekinesis* spell and by the Trump realm, and the three differ in exactly
  two ways — so both are parameters rather than being levelled away. The
  mutation lifts ``level × 10`` and needs line of sight; Sorcery's lifts
  ``level × 15`` and does not, which is what makes Sorcery's the better spell.

  The rest of the rules are Zangband's and each of them is there because leaving
  one out turns a spell with texture into one that just moves things: it refuses
  while you are standing on something, refuses artifacts — *"The object seems to
  have a will of its own!"* — has a weight limit, and given a direction rather
  than a target walks outward until it finds an object and stops at a wall. One
  rule is **not** carried across and is recorded rather than dropped quietly:
  Zangband refuses to fetch out of a vault, and 4.2 marks vault squares
  differently enough that the equivalent test would be an invention.

  **Ten mutations now do nothing**, six activatable and four random.

- **3.54.0** — **The Midas touch comes back** (DEC-52, reversing DEC-48).
  Turning an object into gold was dropped because the mechanic "would have
  exactly one consumer and no prospect of a second". It has three:
  ``alchemy()`` is called by the Midas touch, by Sorcery's *Alchemy* spell, and
  by a random-artifact activation Zangband priced at 10,000. So it is built
  once, for all three, the same way ``FETCH`` will be.

  The numbers are Zangband's, taken from the source. **A third of the object's
  real value** — what a sale prices at, not what the character believes it is
  worth. **Divided before the quantity multiplies it**, so ten objects worth two
  gold each pay nothing rather than six; that is integer division doing what it
  does. **Capped at 30,000.** **Artifacts refuse**, because one-of-a-kind items
  are not currency. And **a worthless object becomes fool's gold** — destroyed,
  nothing paid.

  One deliberate divergence: Zangband multiplies in a signed 32-bit value and
  caps afterwards, so a stack valuable enough wraps negative and pays nothing.
  It needs a cost above about sixty-five million to reach and Zangband's data
  has none, so the bug is latent there rather than live — but a cap that can be
  jumped is not a cap. Found by a test written to pin the cap, which failed on
  it.

  **Eleven mutations now do nothing, all deferred and none refused**, and the
  activatable split is 25 working to 7. DEC-48 is kept in the record with a
  reversal notice rather than deleted, because the reason it gave turned out to
  be wrong and that is the part worth being able to read.

- **3.53.1** — **Books for the three new realms, and a stop on DEC-52.** A
  ``sorcery book``, a ``chaos book`` and a ``deck`` — Trump's book-noun is a
  deck, because that is what Trump magic is. An object records its base by name
  rather than by the tval list's numbering, so adding these mid-list is safe.
  A book's *kind* is created by ``class.txt`` itself, so no object needed
  writing.

  The spell import stops on **DEC-52**, which is open. Sorcery's *Alchemy*
  needs the object-to-gold mechanic that DEC-48 refused for the Midas touch,
  and DEC-48's stated reason — that the mechanic would have exactly one
  consumer — is wrong: ``alchemy()`` has three callers in Zangband, one of them
  a realm spell inside this milestone. Building it would reinstate something
  explicitly refused; skipping it would leave Sorcery a spell short with no
  record. Either way it is not a decision to make in passing.

  Also recorded: **the three new realms have to be imported before the four
  mapped ones.** DEC-50 replaces Arcane, Life, Nature and Death outright, and
  a spell with no effect chain does nothing — so replacing a working realm
  before its nineteen-or-so hand-written chains are finished would leave every
  existing caster holding books that cast nothing. A new realm costs only
  whoever chose it.

- **3.53.0** — **Realms are chosen at birth** (PLR-08, PLR-11). A fourth step
  after race and class, offering the realms the *class* allows — which is what
  makes the combination mean anything. The entitlements are Zangband's own, out
  of ``realm_choices1[]`` and ``realm_choices2[]``, and ``player/realm`` checks
  the shipped data against that table across all 8 classes × 2 slots × 7 realms
  rather than against transcribed values.

  A slot offering fewer than two realms is not asked about, because a list of
  one is an entitlement and not a choice: a Ranger's first realm is Nature and
  the question would have one answer. Angband's Druid, Necromancer and
  Blackguard have no Zangband counterpart, so each is entitled to the one realm
  it already studies — which keeps what they can cast exactly as it was.

  The choice is written to the savefile **by name and by count**, as the patron
  and the virtues are, so inserting a realm cannot rebind a saved character to a
  different one. The ``player`` block goes to version 5 and the version 4 reader
  is kept; a save written before realms existed comes back studying whatever its
  class studies, which for every caster in the corpus is what it was studying
  anyway. The corpus is unchanged at **31 loaded, 4 refused**, so the version
  bump broke nothing incidentally.

  Two things worth recording. The realm list arrives back-to-front from the
  parser, so ``ridx`` 0 was Trump and every list shown to the player would have
  run backwards; it is reversed into file order at load. And the character
  panel asserts rather than growing when given one line too many, so adding the
  realms line aborted the game — caught by the birth frontend test, which is
  the pass that exists for exactly that.

- **3.52.1** — **The spell converter, and a correction to the order M9 goes
  in.** No game change. ``zconv.py realms`` slices Zangband's ``magic_info[]``
  by position — 2,464 ``{level, mana}`` pairs, exactly 11 classes × 7 realms ×
  32 spells — and derives the two fields 4.2 stores and Zangband does not:
  ``sfail`` from the base Zangband computes at cast time, ``sexp`` as
  ``5 × book²`` (DEC-51). It prints ready-made ``spell:`` lines, and re-derives
  the per-class realm entitlements from the table as its own cross-check.
  1,376 lines across every class and realm, none breaking either rule.

  **PLR-08 has to come before CNT-10, which is the reverse of the order they
  are listed in.** A spell in 4.2 lives inside a ``book:`` inside a *class* —
  there is nowhere else to put one. Sorcery, Chaos and Trump are realms no
  existing class carries, and DEC-49 deliberately pinned every existing class's
  progression, so until a character can *choose* a realm at birth an imported
  Sorcery spell has no class to belong to. Realm choice first, then the import.

- **3.52.0** — **The savefile guard for the realm import** (DEC-50). No game
  change yet; this is the fence built before the work that needs it.

  ``spell_flags[]`` and ``spell_order[]`` are recorded by flat index across all
  of a class's books, so what index 7 *means* is decided entirely by the data
  files. DEC-50 replaces the spell content of four realms outright — which
  means an old character would load **successfully**, with the right number of
  known spells and the wrong spells. The sheet would look reasonable and the
  game would play wrong.

  The ``player spells`` block goes to version 2 and now carries a fingerprint of
  the class's spell list. On load it is compared, and a character whose spells
  were learned against a different list is **refused** with a message saying so.
  The version 1 reader refuses too — but only when there is something to get
  wrong: a character with no spells recorded, which is every Warrior and every
  caster who has not learned one, is unaffected and loads as before.

  The fingerprint is not written for this one occasion. Every later change to a
  spell list is caught by the game rather than remembered by whoever makes it.

  Of the 35 characters in the test corpus, **four refuse and thirty-one load**.
  The four are listed in ``tests/saves/EXPECTED-FAILURES`` with their reason,
  and the suite fails both if an unlisted file breaks *and* if a listed one
  starts loading again — a manifest that outlives its reason would silently
  excuse the next real break.


M9: magic realms — 1 September 2026
-----------------------------------

- **3.51.1** — **M9 stops on two questions, with the groundwork recorded.**
  No game change. Phase 2 of the realms — importing the spellbooks — is not
  built, because CNT-10 and PLR-12 cannot both be taken literally once four of
  the seven realms already hold Angband's own spells. Both readings are written
  up as **DEC-50**, with what each costs: 96 spells and no savefile risk, against
  ~224 spells and a migration for every casting class.

  **DEC-51** records the spell-experience question and corrects an earlier
  report of it. It was flagged as an eight-fold scale gap on the basis that
  Angband's ``sexp`` runs 2 to 10; it runs **2 to 300**, and the earlier figure
  came from looking only at low-level spells. Measured properly against
  Angband's median at the same spell levels, Zangband's values cluster on 1.0
  and diverge in both directions — 0.33 to 5.6 — because Zangband ties the award
  to the book and Angband to the spell. A balance note, not a conversion
  problem.

  Phase 2's conversion rules are recorded so they are not derived twice. Both
  fields Angband needs and Zangband does not store come out exactly: ``sfail``
  because the two failure formulas are structurally identical after the base,
  and ``sexp`` because both games multiply by the spell's level. The source
  table is parsed positionally — 2,464 ``{level, mana}`` pairs, exactly 11
  classes × 7 realms × 32 — and that slicing is cross-checked against the
  independent ``realm_choices`` tables with **zero mismatches across all 77
  class-realm combinations**.

- **3.51.0** — **Seven realms** (PLR-09, PLR-10, DEC-49). Angband has four
  realms and Zangband has seven, and the two lists overlap by name without
  overlapping by meaning. The four are mapped onto four of the seven by content
  — ``arcane`` keeps its name, ``divine`` becomes **Life**, ``shadow`` becomes
  **Death**, ``nature`` keeps its name — and Sorcery, Chaos and Trump are added.
  Exactly seven, with all of Angband's metadata kept: casting stat, verb, spell
  noun and book noun, which Zangband held as scattered per-class constants.

  **Nothing an existing character can cast has changed.** Realms are not written
  to the savefile — books are matched by tval and sval — so the rename cost
  sixteen ``book:`` lines and nothing else. Every class keeps the same books in
  the same order with the same spells at the same levels; the only difference is
  the internal name of the realm two book sets belong to, which no player ever
  sees. ``player/realm`` pins the shape of all eight casting classes so that no
  later phase of this milestone can disturb one quietly: inserting a book
  anywhere but the end of a class's list shifts every spell a saved character
  knows one place along, and the game would load and look reasonable.

  **This game's Arcane is stronger than Zangband's**, and that is the one place
  the mapping is not clean. Zangband's Arcane was the deliberate weak generalist
  with every book buyable in town and no high-level power; Angband's is the
  Mage's realm with two books of attacks and a dungeon-only Tome of Power.
  Having exactly seven realms requires folding them, and folding keeps the
  stronger.

  Also here: **the third flake of the session, identified and derived rather
  than nudged.** ``a-borrowed-lord-is-kinder`` compared how often a sworn and a
  borrowed Chaos patron reach the cruel end of the reward ladder, over four
  thousand rolls, and required a ratio above 1.5. The bottom quarter of the
  ladder is rare enough that four thousand rolls give only 74 to 99 events, so
  the ratio ranged 1.475 to 2.284 and the bound sat one and a half standard
  deviations out. It failed about one whole-suite run in fifteen, and the run
  that caught it named its own seed — 1604416127 — which is what the runner
  change in 3.50.3 was for. Ten times the rolls puts the bound six standard
  deviations clear, and the arithmetic is in the comment so the next reader does
  not have to redo it.

Testing — 31 August 2026
-------------------------

- **3.50.4** — **One bad crawl, and two wrong accounts of it.** Every archived
  citation in the plans and docs re-checked one at a time — thirty-six of them
  — before M9 begins, because DEC-16 makes those documents a primary source and
  the realm spell lists are M9's immediate input.

  **Twelve were dead, and they have one thing in common.** All twelve carried
  the timestamp ``20220527225941``, and all twelve are spoilers. The 2022-04-20
  crawl of ``www.zangband.org`` captured everything; the 2022-05-27 crawl of the
  bare domain captured the homepage and almost nothing under it. All twelve are
  whole at ``20220420164xxx`` and have been repointed.

  **There is no parking page**, and ``spoilers/life.txt`` is not unarchived.
  Both were recorded here and neither is true: fetched with ``id_``, the
  2022-05-27 root is the genuine zangband.org homepage, and life.txt returns
  13,854 bytes — the complete Life realm spell list — at the timestamp already
  cited for it. It had been written off on a bad availability check.

  **No requirement rests on a document nobody read.** The four that cite a
  spoiler as their basis were checked against the recovered text and all four
  match: nightmare mode is the irreversible birth option BAL-15…17 describes,
  the Ancient and Foul Curse is CNT-15's, random object abilities are CNT-16's,
  and DEC-38 names all sixteen of Zangband's patrons correctly before replacing
  them with nine Lords of the Courts. The citations were stale; the reading was
  not.

  A note on method, because the first pass got it wrong. A throttled fetch from
  archive.org returns zero bytes and looks exactly like an absent document: the
  first sweep declared twenty-one citations dead, and retrying each failure
  three times reduced that to twelve. Nine documents were nearly rewritten out
  of the plan because the archive was busy.

- **3.50.3** — **The same mistake, in the test written to replace it.** The
  fourteen-run check on 3.50.2 flaked twice, and it was neither the code nor the
  doorstep: it was ``a-character-at-sea-still-finds-ground``, added one release
  earlier, asserting ``restored > 200``. Eight rings around the character are
  289 squares only when the character is not within eight squares of the edge
  of the level; nearer the edge the out-of-bounds squares are skipped and the
  count comes in under the floor for a reason that has nothing wrong with it.
  A constant standing in for an invariant — exactly what 3.50.2 removed from
  the doorstep — put back by the test that removed it. It now asserts that
  everything flooded was restored, which is the actual invariant and has no
  number in it.

  **And the seed the runner threw away.** 3.50.2 gave every sampling suite a
  seed line so a flaky failure could be replayed, and it did not work: a suite
  returns zero whether or not its tests passed, so ``run_tests.cmake`` took the
  success branch and discarded the output — seed included — unless ``VERBOSE``
  happened to be set. That is precisely the case the line was added for. The
  runner now echoes a suite's output whenever it reports fewer passes than
  tests, and the next failure named its seed on the first try: 936496483.

  Sixteen consecutive whole-suite runs clean, which is the claim 3.50.2 should
  have waited for.

- **3.50.2** — **A bound set from eight worlds, and the seed that found the
  ninth.** ``the-doorstep-is-survivable`` required the mean wilderness danger
  within six blocks of the starting town to be at most 12 — a figure taken from
  eight worlds where it came out between 1 and 6. Eight was not enough: on seed
  1829551357 it is 14, and the test failed about one whole-suite run in eight
  for six releases.

  The bound was not wrong by a little, it was measuring the wrong thing. On that
  same seed the country beyond twelve blocks averages 27, so the doorstep is
  less than half as dangerous as the wilderness proper and the world is behaving
  exactly as intended. What the requirement says is that danger *climbs* as you
  walk out — a comparison between two parts of one world, true by construction
  because towns are placed in lawful country and law is what danger reads. It
  has no constant in it to be set from too small a sample. Near 14/7/1/1/1
  against far 27 every time, across five worlds including the one that broke the
  old bound; and measuring around an arbitrary block instead of the town fails
  it on most worlds, so it still discriminates.

  **Every sampling suite now reports its seed.** Only ``game/wild`` did, which
  is why its flake took one command to reproduce once the seed was in hand and
  the one in ``object/imported`` was never identified at all. Seven suites
  gained ``test_seed_rng_reported()``, printed unconditionally rather than under
  ``-v``, because a flaky failure in CI is exactly the case where nobody thought
  to ask for verbose output first.

  The rest of the audit found no other test at risk. ``cave/wild``'s terrain
  composition looks like the same shape and is not — it aggregates twenty
  *fixed* seeds and cannot vary. The remaining sampled bounds are all far from
  their means: weird luck's out-of-depth rate is 8.7 standard deviations clear,
  a Vampire's hypnotic gaze 6.5, the patron notice rate 11.9, and the
  cross-habitat rate measures 93–95 against a bound of 70. Those were derived
  rather than guessed, and two of them say so in their own comments.

- **3.50.1** — **The Midas touch is dropped** (DEC-48). Turning objects into
  gold needs an effect that destroys an object for money, which 4.2 has not
  got — and building one means settling what fraction, of what valuation, and
  what that does to the shops pricing against the same numbers. That is an
  economy mechanic with exactly one consumer, for a mutation that saves the
  player a walk back to town.

  Recorded as a rejection and not a deferral, so it is not picked up again by
  someone reading the deferred list as a queue. ``mutmap.toml`` gained a
  ``reject`` key beside ``defer`` for it — the same split ``objmap.toml``
  already had — and the conversion report lists it under REJECTED.

  The consequence reaches the screen: the power menu now shows six of its
  unusable entries as *not yet* and this one as **dropped**, and says so when
  it is chosen. *Not yet* is a promise, and it is not one this game intends to
  keep. The mutation is still gained, described and saved, on DEC-44's
  reasoning that a mutation vanishing from a savefile is worse than one that
  does nothing.

- **3.50.0** — **Closing M8: the clearing form, and six brackets that lied.**
  Four items the audit turned up after the milestone was declared complete.

  **The converter read only half of Zangband's flag syntax.** `SET_FLAG(of_ptr,
  TR_X)` has a counterpart written `flags[n] &= ~(TRn_X)`, with the word index
  baked into both the subscript and the macro name so it matches nothing the
  parser looked for. Three mutations use it and all three were kinder here than
  in Zangband: rotting flesh is meant to stop a character regenerating, and the
  panic-hit and warning mutations to stop them resisting fear — whether that
  regeneration or resistance came from another mutation or from a ring they are
  wearing. Reading the clearing form across **all seven** parsed regions of the
  Zangband source turns up nothing else: it appears in ``player_flags()`` and
  in one line its own authors had already commented out.

  Fixing the pattern exposed a second bug of the same kind. Both block regexes
  closed on a fixed indentation and were blind to brace depth, so the
  chaos-gift test — which sits one tab shallower than the mutation blocks
  around it — ran past its own closing brace and claimed a flag belonging to
  vampirism. Both now match the closing brace to the opening one. This is the
  third brace-blind read of this source; the shape is now shared whether or not
  a region happens to be uniform.

  **Three mutations stop ordinary food feeding you**, which reached no data file
  and no code at all. A beak, a mouth that eats rock and a taste for blood all
  set Zangband's ``TR_CANT_EAT``, whose eat command leaves such a character a
  twentieth of what they swallow. Scoped to edible things, which is Zangband's
  own scope — a potion of Cure Light Wounds still nourishes as much as it ever
  did, and the test asserts that half too, because dropping the scope passes
  every other check.

  **A mutation's bracket is now generated from its effects.** Six descriptions
  named some of what they do and not the rest, and the worst of them — a living
  computer brain — advertised four points of intelligence and wisdom and said
  nothing about the vulnerability to electricity. Curating six strings would
  have left the seventh to be found the same way, so the text is derived:
  Zangband's sentence, and a bracket built from the entry's own fields. It
  costs the occasional redundancy (*completely fearless (immune to fear)*) and
  buys never shipping a description that hides a downside.

  **And the flake this suite carried for six releases.**
  ``find_open_grid_near()`` in ``game/wild`` searched eight rings for a square
  that is empty and not damaging, and ``square_isdamaging()`` is true of deep
  water — of which this wilderness holds some 330,000 grids. A character
  generated in open ocean has no acceptable square in the 289 that eight rings
  cover, so two tests failed about one whole-suite run in eight and never once
  in two hundred runs of the suite alone. It falls back to the whole level now,
  which cannot fail on a level with one open square on it. A new test floods
  the surroundings deliberately, so the case is exercised every run rather than
  one in eight; removing the fallback fails it and both historic offenders.

- **3.49.6** — **The mutations page drew over the stat table**, so ``WIS``
  read as ``IS`` and ``DEX`` vanished. The stat panel starts at column 42 and a
  mutation's description runs to 57 characters; the two cannot share a screen.
  The page takes the whole screen now, which it needs anyway — a character can
  carry far more mutations than the sheet has rows.

  Found from a screenshot, not from a test. The check that passed had grepped
  for the description being *present*, which it was, on top of the stat labels.

  With it: the page is in :doc:`screenshots`, captured from the running game
  like the others (``ZTK_SHOT_MUTATE``), and :doc:`features` no longer lists
  mutations under *Not yet* — M8 landed.

- **3.49.5** — **The character sheet shows mutations.** A third page, reached
  with ``h`` from the sheet like the other two, listing what chaos has made of
  the character — or *"Chaos has not touched you."*

  Reported from play: a Sprite Mindcrafter whose message log was announcing
  *"You mutate!"* and *"Electricity starts running through you!"* while the
  sheet said nothing at all. Mutations reached the character dump and nowhere on
  screen; the power menu lists the activatable ones, and the passive ones — most
  of the ninety-six — could only be seen by writing a file and reading it.

  Two things went with it. Going backwards through the sheet used
  ``(mode - 1) % INFO_SCREENS``, which is ``-1`` in C rather than the last page,
  and worked by accident with two screens; with three it would have made the new
  page unreachable in that direction. And the dump listed mutations by walking
  the linked list, which is *reverse* file order despite the comment above it
  claiming file order — the list is built backwards and renumbered. The dump,
  the new page and the wizard menu now all read in ``mutation.txt`` order, so
  they can be compared against each other.

- **3.49.4** — **Mutations can be granted by hand.** ``^A`` then ``U``, under
  *Debug → Player*: all ninety-six listed, the character's own marked ``[*]``,
  select to toggle.

  Chaos hands out mutations and the player does not choose them, which makes
  everything that touches them hard to test on purpose. Polymorph Self is the
  worst case — you cannot ask for the mutation that grants it, you cannot see
  the passive ones you have (the power menu lists only activatables, and the
  full set reaches the character dump and nothing on screen), and the effect
  itself does nothing about 60% of the time by design.

  ``U`` rather than ``M`` because every ``Dbg*`` list shares one key namespace,
  and ``M`` is *Write map* in another submenu. That is an assert in
  ``cmd_init()``, so the clash is a crash before the title screen rather than a
  menu that quietly misbehaves — which is how it was found.

- **3.49.3** — **A cheat for powers that will not fire.** ``cheat_powers``, on
  the ``=x`` screen: every racial and mutation power succeeds, and the listing
  shows ``0% to fail`` because the menu and the roll ask the same function.

  Reported from play: a level 21 Beastman Chaos-Warrior looking at Polymorph at
  ``20 hp, 95% to fail``. Worth recording *why* it is 95, because it is neither
  the difficulty (18) nor the level (21 against a power level of 18) — a
  character short of mana is charged 5% per point of the shortfall, and a class
  with no spell points is short by the entire cost. 20 × 5 = 100, which buries
  every other term and clamps to the ceiling.

  So the blood price that exists precisely so a spell-less class can use a
  power at all is cancelled by a penalty for needing it: ``player_use_power()``
  lets a Warrior pay 20 hit points, and ``player_power_chance()`` then makes it
  fail 95 times in 100. Whether that penalty should apply to a character paying
  in blood is a balance decision and has not been made — the test records the
  present behaviour and will notice when it changes.

Savefile compatibility — 30 August 2026
----------------------------------------

M8: mutations — 31 August 2026
------------------------------

- **3.49.2** — **A shared helper that overflowed on its twenty-seventh call**.
  ``menu_dynamic_add_label()`` writes a row's shortcut key into the caller's
  label buffer at the row's own index, and did so without knowing how long that
  buffer was. Every dynamic menu in the tree allocated it with
  ``string_make(lower_case)`` — twenty-six letters and a terminator — so the
  twenty-seventh row overwrote the terminator and the twenty-eighth ran off the
  end of the allocation.

  **This is an Angband defect, not one this port introduced**, and it is latent
  upstream rather than live: an audit of all twenty dynamic menus found the
  largest upstream one is eleven rows, and every row of every fixed list
  supplies its own key, so the pre-filled letters are entirely overwritten and
  the buffer's length never matters. Four menus in this game are driven by data
  instead — the magetower's destinations (up to 48), the Chaos Tower's and the
  power list's mutations (up to 89), and the quest giver's (capped at 16) — and
  three of those can pass twenty-six. The magetower had already hit it and
  fixed it locally; the other two were fixed by clamping in 3.49.0, which was
  the wrong place to be defensive.

  The bound now lives on the helper. ``struct menu`` carries how many bytes of
  its label buffer may be written, ``menu_dynamic_labels()`` is the one way to
  get such a buffer, and all twenty call sites use it. A menu that never asks
  for one has a bound of zero and cannot be written into at all, so the unsafe
  thing a future caller might do is now the thing that does nothing. Fifty-one
  keys are available rather than twenty-six, from ``all_letters`` less ``q`` —
  skipped because these menus end with a "no thank you" row keyed ``q`` and the
  selection scan takes the first match, which is how the magetower's
  seventeenth destination came to be unvisitable. **Rows past the bound keep
  their place and lose only their key**; refusing the row would silently shorten
  a long menu, which is the more dangerous failure of the two.

  Long dynamic menus still draw past the bottom of the terminal —
  ``menu_dynamic_calc_location()`` sets ``page_rows`` to the row count and
  nothing scrolls. That is a separate upstream limitation, unchanged here, and
  the magetower has lived with it at 48 destinations since M5.

- **3.49.1** — **The manual is part of the gate.** ``scripts/check-build``
  builds it as a fourth pass with ``-W``, so a broken directive or a bad cross
  reference fails locally instead of failing the job that publishes
  zangbandtk.com. Missing Sphinx stops the gate and prints the two commands
  that fix it, rather than skipping the pass.

  The virtualenv had not gone anywhere. ``docs/Makefile`` took ``sphinx-build``
  from ``PATH``, nothing ever put ``.venv-docs`` there, and ``make -C docs
  html`` therefore failed with "sphinx-build: command not found" on a machine
  with a working Sphinx sitting in the tree. That message reads as "Sphinx is
  not installed", and was believed — so the manual went unbuilt across several
  releases that reported it as building. ``docs/Makefile`` now looks for the
  virtualenv before falling back to ``PATH``.

- **3.49.0** — **How you come by a mutation, and how you get rid of one**
  (PLR-14, PLR-34, DEC-24). Every documented acquisition path that does not go
  through a magic realm now works: a Lord of the Courts replaces one favour in
  six with a mutation (the DEC-38 carry-over, filled in), the chaos gift makes
  a Lord take an interest in a character who never swore to one, unresisted
  chaos changes you one time in three, and Polymorph Self rerolls what chaos
  has made of you. The two remaining routes are Chaos and Death realm spells
  and wait for PLR-08.

  Three new effects carry all of it — ``GAIN_MUTATION``, ``LOSE_MUTATION`` and
  ``POLY_SELF`` — so a patron's ladder, a potion, a breath and a building can
  each reach the same machinery from data.

  **The Chaos Tower is built** (DEC-24). Zangband names it in ``t_info.txt``,
  routes it through a Lua hook and ships no script to fill the hook, so it had
  a building, a door and no behaviour; ``spoilers/mutation.txt`` lists it as one
  of only six ways to lose a mutation, which is why DEC-24 kept it when it cut
  the Casino and the Weaponmaster. Great cities only, 2500 gold, and it is the
  only route that lets the player *choose* which mutation goes.

  **The potion of New Life came back.** CNT-11 deferred it on the single ground
  that "mutations are unbuilt", and M8 built them. Half of Zangband's potion
  survives — ``cure_all_mutations()`` does, ``do_cmd_rerate()`` does not,
  because 4.2 fixes hit points at birth — and its description was overridden to
  stop promising the half that no longer happens.

  **A buffer overflow that mutations made reachable.**
  ``menu_dynamic_add_label()`` writes ``label_list[m->count]`` into a copy of
  ``lower_case``, which is twenty-six characters and a terminator. Nothing in
  4.2 ever built a menu longer than that. A character can carry eighty-nine
  mutations at once — ninety-six less the seven the cancelling pairs make
  unreachable together — so the power list and the Chaos Tower could both run
  off the end of the allocation. Both now stop at twenty-six and say how many
  they did not show, rather than truncating in silence.

- **3.48.0** — **The mutations that act on their own** (PLR-14, PLR-35).
  Twenty-one of the twenty-seven random mutations now fire on their own timer,
  and all five melee mutations land an extra blow in the attack round. That
  completes what a mutation *does*; the ways you come by one are next.

  Each random mutation is rolled separately every turn, so three of them are
  three chances rather than one, which is Zangband's arrangement. Every one is
  suppressed by anti-magic except cowardice — Zangband tests the flag inside
  sixteen of the seventeen blocks that could have it and pointedly not in that
  one, because being too frightened to act is not sorcery. That reading was
  kept rather than tidied into a blanket rule.

  **The melee dice were written the wrong way round in Zangband, all five of
  them.** ``natural_attack()`` fills in ``dss`` and ``ddd`` and then calls
  ``damroll(ddd, dss)``, whose parameters are ``(num, sides)`` — so a scorpion
  tail is described as "3d7" and rolls **7d3**, and an elephantine trunk is
  described as "1d4" and rolls a flat **4d1**. Every one of the five hits
  harder in the code than in its own text: 14 against 12, 9 against 7, 6
  against 5, 4 against 2.5, 7.5 against 6. DEC-20 puts the source ahead of the
  documentation on algorithm, so the dice are the code's and the descriptions
  have been corrected to match — they are the only place a player ever sees
  these numbers.

  Six random mutations are deferred with reasons: wraith form (4.2 has no
  incorporeal player state), the warning (a piece of user interface rather than
  an effect), the two hit-point/spell-point exchanges (no effect reads both
  pools), losing a mutation at random (Phase 5's removal paths), and the chaos
  gift (Phase 5's patron).

- **3.47.0** — **Twenty-three mutation powers, and nine honest gaps**
  (PLR-16). The activatable mutations join the same list as racial and class
  powers, after them, and use the same machinery — the same failure roll
  against a stat, the same fall back to hit points when the mana is short.

  There is no mechanical route from Zangband's ``mutation_power_aux()`` to a
  4.2 effect chain: it is nine hundred lines of hand-written C in which every
  power calls whatever it happens to need. So the translations live in
  ``tools/zconv/mutmap.toml``, one judgement per mutation, each carrying the
  source line it was read off and what the translation costs — and the
  converter emits them, so ``mutation.txt`` stays generated.

  **Nine cannot be expressed and say so.** Telekinesis, swapping places,
  sensing curses, the Midas touch, growing molds, weighing magic, sterility and
  the launcher all need machinery 4.2 has not got, and building it to carry one
  mutation would be the wrong way round in most cases; Polymorph Self is
  deferred to the acquisition paths it belongs to rather than away. All nine
  appear in the power list marked *not yet* rather than being hidden, because
  the character sheet describes them and a player would come looking.

  Level-scaled radii became ``power-when`` bands, which is what DEC-37 added
  them for: Zangband's spit-acid radius is ``1 + level / 30``, so it is one
  band to level 29 and another after, not an average.

  **Twelve descriptions were advertising charisma**, which 4.2 removed in
  4.2.0. A silly squeak read "(-4 CHR)" and is in fact inert; warts read
  "(-2 CHR, +5 AC)" when only the armour was real. The converter now takes out
  the charisma term and the punctuation holding it in place, and nothing else.

- **3.46.0** — **The thirty-two standing mutations act** (PLR-14, PLR-15,
  PLR-17). A continuous mutation is now simply true of your character: stats,
  armour, speed, searching, stealth, saving throw, resistances,
  vulnerabilities and object flags all land through ``calc_bonuses()`` the way
  a shapechange does, and the character dump gains a **[Mutations]** section.

  The modifiers come out of ``mutation_effect()`` rather than out of
  ``spoilers/mutation.txt``, and the difference is not small. The spoiler gives
  the headline of each mutation and stops, and the headline is generally the
  good half: superhuman strength is "+4 STR" there and +4 STR, -1 INT, -1 WIS
  in the code; being puny is "-4 STR" and also **+2 DEX**; a moronic mind is
  "-4 INT/WIS" and never mentioned as conferring immunity to fear and
  confusion; iron skin is +25 AC and -3 DEX rather than -1. Built from the
  documentation, every good mutation would have been better than Zangband's and
  every bad one kinder.

  Two things needed measuring rather than assuming. Zangband's stealth and
  searching skills are on the *same* scale as 4.2's — of the nineteen races
  both games share, stealth is identical for seventeen and searching for
  twelve — but ``calc_bonuses()`` multiplies ``OBJ_MOD_SEARCH`` by five on its
  way into the skill and leaves stealth alone, so extra eyes are ``SEARCH[3]``
  and not ``SEARCH[15]``. And saving throws are a skill in both games but not
  an object modifier in 4.2, so they get their own field — with a scale as well
  as an amount, because magic resistance is ``15 + level/5``.

  **Two mutations are inert and are meant to be.** A silly voice and an
  illusory normal appearance moved charisma and nothing else, and 4.2 removed
  charisma in 4.2.0. Both are still gained, described and saved; neither does
  anything. Left in rather than dropped, because a mutation that disappears
  from a savefile is worse than one that does nothing.

- **3.45.0** — **Ninety-six mutations, and the model that carries them**
  (PLR-13, PLR-36, PLR-37, PLR-38). Zangband built mutations with no data file
  at all: the table is a C array in ``tables.c`` and the selection weighting
  exists only as the widths of the case runs in a switch. ``zconv mutations``
  reads both and writes ``mutation.txt``, so this is generated like everything
  else rather than hand-copied.

  Reading the source rather than the spoiler turned up three things the
  documentation never mentions. **Three mutations have prerequisites** — the
  Midas touch wants a thousand gold per level in hand, and a silly voice and
  elemental vulnerability want three mutations already, which is chaos
  compounding on itself. **The race affinities are not uniform**: a Vampire
  takes hypnotic gaze six times in ten and a Beastman polymorph self only one
  in ten. And **the regeneration penalty the spoiler warns about is not in
  2.7.5** — it was a 2.2.2d mechanic its authors removed, and reinstating it
  would not be finishing an unfinished feature (DEC-45).

  Four kinds rather than the plan's three: the five melee mutations sit in the
  same machine word as the random ones and behave nothing like them (DEC-44).

  Charisma bites again. Three activatable mutations cast off ``CHR``, which 4.2
  removed in 4.2.0; all three work on a mind rather than the world — the
  spoiler compares hypnotic gaze to the Mindcrafter's Domination by name — so
  they cast off wisdom here, recorded as a substitution in the conversion
  report.

  The savefile block is written and read by name and by count, and the player
  block goes to version 4 with three older loaders kept.

M8: the virtues, finished — 30 August 2026
------------------------------------------

- **3.44.3** — **The last gap between the local gate and CI is closed, and it
  found nothing.** Homebrew's GCC now sits beside the system clang as
  ``gcc-16`` — not as ``cc``, which stays clang for the macOS build the way
  ``python3.13`` sits beside the system Python. ``scripts/check-build`` runs a
  third pass with it, at ``-Werror -Wvla -Wlogical-op``.

  **The whole tree is clean under it**: 214 sources, zero warnings, zero
  errors. ``-Wlogical-op`` turned up no real defects, which is worth saying
  plainly rather than dressing up.

  The pass earns its place all the same, and this was checked rather than
  assumed. ``depth > 0 || depth > 0`` is an error to GCC and *silent* to clang
  with ``-Werror`` — introduced deliberately, the clang builds returned 0 and
  the GCC build returned 1. One nuance: the two CI jobs that ask for
  ``-Wlogical-op`` do **not** pass ``-Werror``, so CI prints such a warning and
  builds on. Locally it stops the gate. That is one step stricter than CI on
  purpose, and affordable only because the tree is clean of it.

  Also fixed: the script sent build output to ``/dev/null``, so a failure gave
  an exit code and no reason — the compiler talking to nobody again, by a third
  route. It is quiet while it works and prints everything the moment it does
  not.

- **3.44.2** — **The gate covers the build it was missing, and 3.44.1's
  account of the gap was wrong.** That entry framed the residual risk as
  clang against CI's GCC. It is not: the **macOS** CI job builds with
  ``env OPT="-Werror" make -f Makefile.osx`` and the same clang on this machine
  rejects the bad pointer exactly as GCC did. There was never a parity problem
  to reason about — the local build was the identical compiler run without the
  flag that makes a warning fatal.

  So ``scripts/check-build`` now runs **both** build systems, because CI does
  and they are not interchangeable. ``Makefile.osx`` asks for ``-Wshadow``,
  ``-Wwrite-strings``, ``-Wmissing-prototypes``, ``-Wnested-externs`` and
  ``-Wunused-macros``; CMake asks for ``-pedantic``; and they compile to
  different C standards, c99 against gnu99. A defect can pass either alone.
  Both are clean over the whole tree — 214 sources each, and the same 214.

  This also collapses the two failures of the last day into one. The grep that
  did not match ``error:`` and the build that was not asked for ``-Werror``
  are the same mistake twice: the compiler was reporting a problem and the
  local check was not reading the answer.

- **3.44.1** — **A pointer of the wrong type, and the gate that should have
  caught it.** ``virtue_note_kill()`` passed a ``monster_race`` to
  ``monster_is_living()``, which takes a ``monster``. Clang built it and the
  undefined behaviour ran; GCC with ``-Werror`` on a Linux runner refused it.
  The predicate reads only race flags, so the fix is a ``race_is_living()`` for
  callers that have a race and no instance — ``monster_is_living()`` now
  delegates to it, and the substitution keeps the semantics it was given: the
  living check, on the unique branches only.

  **The local build was never running CI's flags.** ``cmake -S . -B build``
  takes the defaults; every Linux job configures with ``-Werror``. That gap let
  two defects through in a day — this one, and two invented identifiers a few
  hours earlier that were reported as a clean build because the output was
  being filtered with a pattern that does not match what clang prints.
  ``scripts/check-build`` now configures with CI's flags and lets its exit code
  be the answer. Run over the whole tree it finds nothing else of the kind.

  It deliberately does not go beyond CI: ``-Wshadow`` alone objects to a dozen
  places in the existing suite that every Linux job builds happily, and a gate
  that fails on things CI accepts is one people learn to ignore.

  The kill hook also gained the test it did not have. Every one of the suite's
  other tests passed with the bad pointer in place, because nothing called the
  function.

- **3.44.0** — **Zangband's unfinished feature, finished** (PLR-18 to PLR-21).
  Topi Ylinen wrote the virtue system in 1998 and it never worked: by 2.7.5-pre1
  there were **168 places that wrote a virtue and none that read one**, and the
  knowledge screen that would have shown them was commented out with the note
  *"Display virtues option is always left out"*. Eight counters per character,
  moving with everything they did, and nothing looked at them for seven years.

  The selection is kept because it is the part that did real work: a character
  is measured against **eight of eighteen**, chosen at birth by class, then
  race, then the realm they cast from, deduplicated, and padded from a weighted
  table. Two Warriors of different blood are asked different questions. The
  tables live in ``class.txt``, ``p_race.txt`` and ``realm.txt`` rather than in
  a switch statement, so what a Necromancer is measured against is legible
  without reading C.

  **Two things read them, which is the whole point.** A Lord of the Courts of
  Chaos reaches for the bottom of its reward ladder less often for a character
  strong in Chance and Individualism, and more often for one strong in Harmony
  and Temperance — a Lord of Chaos is not impressed by a well-run life. And the
  dream at the inn runs truer for Enlightenment and Knowledge and darker for
  Unlife and Chance. Both sit on mechanisms that already existed.

  **The writers are sized by coverage rather than by census** (DEC-43). Porting
  all 169 of Zangband's nudges was one option and porting only what the two
  consumers read was the other; this is neither. The rule is that *every virtue
  a character can be given must be one their play can move*, because a Warrior
  carrying eight counters that nothing moves and nothing reads is the condition
  Zangband shipped in, reached by a different road. Nine hook sites cover all
  eighteen: what you kill, striking something asleep, a chaotic weapon
  discharging, digging and clearing trees, hasting, making yourself
  invulnerable, enchanting, mapping a level you have not walked, and collecting
  a bounty.

  One substitution, marked as one. Zangband's death block asks three times
  whether the dead thing was ``GOOD``, and 4.2 has no such flag — nor is it
  recoverable, Zangband having put it on 230 monsters running from Farmer
  Maggot to Mughash the Kobold Lord. ``monster_is_living()`` stands in on the
  unique branches only; the rest is dropped rather than approximated. Without
  it Unlife would have had no writer that raises it, and a Necromancer would
  carry a counter that only ever fell.

  The savefile block is written and read **by count**, and the player block goes
  to version 3 with version 2 kept as a loader — so the thirty-five characters
  in ``tests/saves`` still open. A reader looping to its own compile-time
  constant is exactly what broke them a fortnight ago.

  **This closes M8's gate.** DEC-39 kept virtues on condition that something
  read them; two things do.

- **3.43.1** — **The Incandescent Globe of Sawall is dropped** (DEC-42).
  Zangband has two artifacts of that name — a light and a helm, both real, and
  neither a draft of the other. It could carry both because it saved an
  artifact by index and put the base object in front of the name; Angband saves
  the name alone and takes the first match, so a character could come back
  holding the wrong one. Renaming was available — three of the nine Chaos
  patrons are of House Sawall — and was declined as not worth it for one
  artifact. The helm stays, because it is the one already shipped and renaming
  a shipped artifact makes every savefile holding it refuse to load. The count
  stays at 51.

  The converter now finds *any* duplicate name among the candidates rather than
  only the one it was told about, and reports an unruled pair as a problem
  instead of quietly importing both.

  One artifact also gained a property it should have had a version ago: the
  Staff **of the Wandering Wizard** carries Zangband's ``SP``, which became a
  mana modifier in 3.42.0 — the artifact file was not regenerated at the time,
  so it had been sitting on a mapping that existed and was not applied.

- **3.43.0** — **A character who has cheated says so.** ``Cheat``, in red, at
  the left of the status line, for as long as that character exists.

  The game has always known. ``noscore`` is set the moment wizard mode is
  entered or a debug command is used, the cheat options each have a hidden twin
  set the moment they are turned on, and neither is ever cleared — that is what
  bars the character from the high score list. None of it showed on screen, so a
  character being walked about invulnerable looked exactly like one that had
  earned its way.

  Which is where this came from. A screenshot of a level 6 character with 1012
  hit points, sent to report a stalking deer, read as a possible bug in hit
  point calculation; it was wizard mode, and the game could have said so.

  Both records are now asked through one function, ``player_has_cheated()``,
  which the score list uses too — so the indicator and the scoring cannot drift
  into disagreeing about whether a character was played straight.

- **3.42.3** — **The white deer bounds out of earshot.** Reported from play a
  second time, as *still stalking me*. The blessing itself was fixed in 3.33.2
  and was working: the deer gave its gift once and shied away from every touch
  after. What it would not do was leave. It bounded ten grids, and a deer moves
  at speed 130 and hears at 40, so ten grids is a distance it undoes in a few of
  your turns — arriving back beside you to be touched and refused, over and over.

  The bound is now ``world:blessing-bound`` (100 grids by default, and tunable
  without a rebuild), chosen to clear the beast's own ``hearing`` rather than
  its hooves. Past that it no longer knows where you are and goes back to being
  a deer somewhere else. It is still neither killed nor removed; you can meet it
  again, and it will still have nothing more to give you.

- **3.42.2** — **Every character saved before today stopped loading**, and the
  app terminated outright on some of them. Thirty-five real savefiles now live
  in ``tests/saves``, and a unit test loads every one of them, saves it again
  and loads it back — because loading alone passes over a fault that only
  shows up on the next save.

  Four faults, all the same shape: *the savefile names something this build no
  longer has.* ``rd_misc`` sized three of its arrays from the counts compiled
  into the running build rather than the counts written in the file, which the
  rest of the loader has always read back correctly — so adding an object flag,
  an element or a modifier invalidated every existing save. ``rd_monster_memory``
  span forever on a monster that had been removed. ``rd_monster`` treated a
  vanished race as fatal to the whole load, which is what actually killed most
  of the corpus. ``rd_trap`` and ``rd_ignore`` will do the same at the first
  renamed trap or object; both are fixed before they fire.

  Setting ``ZTK_SAVE_TRACE`` now names each block as it is read and echoes the
  loader's notes. Every one of these was invisible without it.

M2: the rest of Zangband's objects — 30 August 2026
---------------------------------------------------

- **3.42.1** — **The Scroll of Rumour is dropped rather than deferred**
  (DEC-41). It sat on the deferred list, which reads as a queue, and it was
  never going to reach the front of one: the mechanism is a morning's work and
  the content is 647 individual judgements about which of Zangband's rumours
  are still true of this game. It now sits with the artifact bases under
  *rejected*, where nothing is waiting for anything.

- **3.42.0** — **Three of the objects CNT-11 held back, and two that stay
  held.** A **Ring of Wizardry** needed a mana modifier, which Angband's object
  model did not have; ``MANA`` is appended to the eleven it did. The pval is
  mana *per casting level* rather than a flat bonus
  (``msp += sp_bonus * levels``), which is the part of ``SP`` its name does not
  tell you and the reason the ring is worth a finger at all.

  A **Rod of Havoc** turned out to need no code. ``call_chaos()`` picks one of
  thirty damage types and throws it, and ``effect:RANDOM`` chooses one of the
  effects that follow it — so the whole table is a data entry. 25 of the thirty
  elements exist here; the shapes that could not be expressed, and the five
  elements that do not exist, are named in the conversion report rather than
  approximated.

  A **Scroll of Mundanity** strips an item back to the plain thing it was made
  as. Zangband reset the fields one at a time; 4.2 keeps an item's power in
  runes and curses as well as numbers, and ``object_prep()`` rebuilds all of it
  from the kind — the same operation, in one call.

  **A Scroll of Artifact Creation is not built, and the reason is the
  savefile.** Angband stores an artifact by name and builds ``a_info`` from
  ``artifact.txt`` at startup, so one invented during play would not exist when
  the character was loaded again. **A Wand of Rockets stays held** for the
  reason it was held before: its damage is its own element, ``el_info[]`` is
  written to the savefile by position, and the change reaches every object and
  monster in the game for the sake of one wand.

  **A Scroll of Rumour is waiting on a decision, not on code.** Zangband's 647
  rumours are in the archive and ``hints.txt`` is already the same shape, so
  the mechanism is a morning. A good many of the lines name things this game
  does not have or contradict what it does, and a scroll whose purpose is to
  tell the player something true is the wrong place to ship text that is false
  — which makes it curation under DEC-19 rather than conversion.

- **3.41.0** — **Three flags that were waiting on nobody, and two that were
  waiting on more than they said.** ``CAN_FLY`` and ``CAN_SWIM`` reach 117 and
  91 imported monsters. Angband gates damaging terrain on one resistance flag
  named by the terrain — deep water asks for ``IM_WATER`` — which answers
  whether a creature can survive a grid and not whether it can keep out of one.
  A raven does not resist drowning; it never touches the surface. Until now
  every bird and every swimmer in the import was stopped by the first river it
  met, which is invisible from the outside, because a monster that will not
  cross simply goes somewhere else.

  ``PATRON`` gives a Lord of the Courts an interest in whoever carries it,
  which the ``(Chaotic)`` ego does. Building it turned up something missing
  from work already shipped: Zangband's condition for a level reward is
  ``TR_PATRON || (one_in_(7) && TR_STRANGE_LUCK)``, and the Ring of Fate's
  fifth effect — the only one that is not about critical hits — had been left
  out when ``STRANGE_LUCK`` was built two versions ago. It is in now.

  **The cruelty runs the opposite way to the fiction.** Zangband writes
  ``nasty_chance *= 2`` for a character with no patron of their own, and that
  is the denominator of a one-in-N roll, so a Lord that was never yours is
  *half* as likely to reach the bottom of the ladder. This shipped backwards
  first, and the test written alongside it asserted the backwards version —
  caught because the wrong direction collapsed two of the four rates in an
  older test onto each other and made it fail about one run in eight.

  **``WILD_SHOT`` and ``WILD_WALK`` are not built, and the reason they were
  recorded as unblocked was wrong.** Both cancel a terrain penalty, and this
  game imposes neither: there is no terrain term anywhere in Angband's combat
  for ``WILD_SHOT`` to waive, and ``WILD_WALK`` removes the cost of climbing a
  mountainside that is a wall here rather than slow ground, and the poison of a
  swamp that is not a terrain type at all. The blocker was never the wilderness
  arriving. One ego goes with them: ``of the Wild`` had ``WILD_WALK`` and
  nothing else, so it was reaching the game as boots that granted nothing.
  The converter now drops any ego left with no property at all, by rule rather
  than by name, so it returns on its own the day the flag is built.

- **3.40.3** — **Bookkeeping that had drifted far enough to mislead.** The
  requirement coverage table accounted for 91 of the 109 requirements and said
  in a footnote that seven were unassigned. Rebuilt rather than patched, with
  the arithmetic written down so it can be checked: 104 scheduled, one standing
  rule, two built and never scheduled, two closed by decision. Three
  requirements — WLD-04a, WLD-04b and WLD-08a — appeared nowhere in the plan at
  all despite having shipped in M4.

  ``BAL-07`` is recorded as met, with **DEC-40** for the method. The mapping
  from Angband 2.8.1's ``sleep`` to 4.2's ``sleepiness`` was derived from the
  434 monsters both versions carry rather than assumed, and the interesting
  part is what it found: for 13 of the 21 distinct source values the recovered
  figure *is* the source figure, and 275 of the 434 monsters carry the same
  number in both. 4.2 never rescaled sleepiness — it edited individual
  monsters. The scale was identity with noise on it, and the median is what
  takes the noise off.

  The ``# Source:`` line in the five generated data files is now written
  relative to the top of the tree instead of absolutely, so regenerating in a
  differently-named checkout produces the same bytes. ``check-build-lists``
  fails if an absolute path comes back. It cannot check the stronger property —
  that the shipped file is what the generator produces today — because the
  Zangband sources are not in the repository, so that comparison is only
  available to someone who has them.

- **3.40.2** — **A potion that no game could ever have produced.** Angband
  builds one row of its object allocation table per dungeon level, from zero to
  ``obj-make:max-depth``, and clamps every request into that range. Zangband's
  dungeon went deeper than the table does, so its Potion of Invulnerability
  arrived at ``alloc:11:105 to 105`` — five rows past the end of a hundred-row
  table, present in the data and unreachable in play. Two more objects had
  ranges that ran past the end without being stranded by it. The converter now
  clamps both ends of every allocation band to the table's last row, reading
  the ceiling from the game's own ``constants.txt`` rather than carrying a copy
  of the number.

  **The deeper half of this was investigated and not done.** The dungeons run
  to 127 while the object table stops at 100, so the bottom 27 levels generate
  their loot from the depth-100 row — which reads like a bug and is not one.
  Widening the table means every object whose range ends at 100 stops being
  generated below it, and that is 343 of the 434 kinds that have an allocation
  line: **nothing at all would be generated at depth 127**. "To 100" is
  Angband's way of writing "to the bottom" throughout its own data, and the
  clamp is what implements it. The flat deep end is not the table being too
  short; it is that no object in either game is authored below depth 80.
  Changing it means authoring deeper objects, which is content rather than a
  fix.

- **3.40.1** — **Three imports that were Angband's own content under new
  names.** CNT-11 found that comparing object kinds by name let Zangband's
  renames through as if they were new; the same scrutiny applied to the
  monster, artifact and ego imports found three more, and each file needed a
  different notion of identity to find them.

  Ego indices are 98.9% stable between Angband 2.8.1 and Zangband, so the index
  *is* identity there, and the single divergence was a rename: ``of
  Levitation`` is Angband's ``of Slow Descent`` — same slot, same lone
  ``FEATHER`` flag — which Angband still ships. Artifact indices are 69%
  stable, and every one of the 35 divergences turned out to be Zangband putting
  one of its own artifacts in an inherited numbered slot: Frakir for the Ring of
  Barahir, the Crown of Chaos for the Crown of Morgoth. Those are replacements,
  not renames, and were correctly imported. **The artifact row is clean.**

  Monster indices are 0.5% stable — Zangband renumbered the whole bestiary — so
  identity had to come from somewhere else, and what survives a rename is the
  *description*, which is hand-written prose. 33 imported monsters keep a 2.8.1
  monster's paragraph word for word. Thirty-one of those are Zangband writing a
  much deeper monster on inherited text, or boilerplate that several Angband
  monsters already shared. Two were renames: ``hobo`` is the boil-covered
  wretch and ``raving lunatic`` is the village idiot, identical in glyph,
  colour, speed, armour, depth and experience, and both still in Angband under
  the older name.

  **The bestiary is 387 imported monsters rather than 389, and there are 17 ego
  types rather than 18.** The total of 1013 has not moved, because it was
  already two out of date.

  One artifact was also being lost silently rather than skipped. Zangband has
  two called "of Sawall", and the converter read artifacts into a dictionary
  keyed by name, so one overwrote the other. It now reads them as a list and
  defers the second with the reason recorded: Angband writes an artifact's name
  into the savefile and reads it back with the first exact match, so it cannot
  carry two of one name. Giving that one a name of its own is a content
  decision rather than a conversion one.

  .. warning::

     **A savefile holding a hobo, a raving lunatic, or an item of Levitation
     will refuse to load.** Angband stores monsters and egos in savefiles by
     name and reports "Monster race ... no longer exists!" when the name has
     gone. The failure is loud rather than silent, but the character is not
     recoverable without putting the entries back.

- **3.40.0** — **Eighty-two object kinds, and the three properties they needed.**
  Zangband's own objects, which until now were the one part of its content that
  had not come across: thirteen swords, nine polearms, eight hafted weapons,
  nineteen pieces of armour, twenty-eight rings and amulets, and the
  consumables among them that Angband has an effect for. A new ``objects``
  subcommand in ``zconv`` produces them, and the data file it writes is
  generated rather than edited, like the other three.

  **The count in the plan was measured by name, and names lie.** Zangband
  renamed twenty-six of the objects it inherited from Angband 2.8.1 without
  changing what they are — its Ring of Skill is Angband's Ring of Accuracy, its
  Scroll of Enchant Weapon Deadliness is Enchant Weapon To-Dam, its Ring of
  Levitation is Feather Falling — and ten of those are still in Angband today
  under the older name. Importing by name puts both in the game. The converter
  compares the slot an object occupies instead, which is its actual identity,
  and that also settles the reverse case: four slots where Zangband kept the
  number and put a different object in it, which *are* new content.

  Sixteen more were refused for being artifact bases rather than objects. Seven
  of them are called "Ring", exist only so an artifact has something to hang on,
  and importing one puts a plain ring in front of the dummy the Ring of Barahir
  needs — after which ``artifact.txt`` will not parse.

  **A Ring of Fate is not a lucky charm.** Its ``STRANGE_LUCK`` sharpens every
  melee critical by half again, including the ones monsters land on you, and
  gives one monster in thirteen a chance of arriving from up to forty levels
  deeper than the floor should allow. Zangband's own comment on the third of its
  four effects reads "Luck isn't always good for you...". Also new:
  ``PSI_CRIT``, which spends one to three mana per critical and does nothing on
  an empty pool, and ``NO_MAGIC``, which stops you casting outright.

  Nineteen objects were deferred with the reason recorded, not dropped: ten
  statues and a figurine whose names interpolate a monster Angband objects
  cannot refer to, two more that need pets, and the rest waiting on mutations, a
  chaos-effect table, a mana modifier, a rumour list, or an element that cannot
  be added without changing the savefile layout.

  Three older faults surfaced on the way, all in shared conversion tables and
  all now fixed in the generator. Zangband's ``LITE`` is a boolean ``+1`` light
  radius, not one of the properties its single pval scales — read as one, it had
  been giving the Crown of Chaos a light radius of **125**, and thirteen other
  artifacts too much. ``NO_MAGIC`` on an artifact had been mapped to Angband's
  *impair mana recovery* curse, which is a different thing and is not even legal
  on an amulet. And an item carrying both an immunity and a resistance to the
  same element emitted both, in that order, so Angband's parser assigned the
  weaker one last and quietly downgraded the immunity.

M2 and M3: the flags that were left — 29 to 30 August 2026
------------------------------------------------------------

- **3.39.0** — **The last of Zangband's flags, and two that were never
  flags at all.** ``QUANTUM``: one monster that half your blows pass through,
  that takes half its turns, and that now and then simply stops existing.
  ``GHOUL_TOUCH``: what you strike bare-handed falls asleep. ``RETURN``: a
  thrown weapon comes back nineteen times in twenty. ``LUCK_10``: a better
  saving throw. ``EASY_ENCHANT``: twice the enchantment attempts, and no
  artifact resistance.

  Two of the eight turned out not to be mechanisms. ``SENSE`` is Zangband's
  data-file name for the searching skill — its own C calls the same flag
  ``TR_SEARCH`` — and ``THROW`` is an item meant to be thrown, which Angband
  already has as ``THROWING``. Both had been filed as things to build on the
  strength of their names, and both were being dropped from the conversion
  instead: the rename alone restores a search bonus to eight artifacts and two
  egos.

  ``PASS_WALL`` is refused rather than deferred. Angband has no form of player
  wall-movement to hang it on, so carrying it would mean building wall-phasing
  rather than reading a flag.

  **That is the end of the flag work.** ``flagmap.toml`` has nothing left to
  implement at all, and ``objflagmap.toml`` has only ``PASS_WALL``, kept there
  with its reason rather than pretended away. Everything else in either is
  deferred: the pet allegiance model, which is M10, three ego flags whose
  blockers have since shipped, and the habitat tags, which this game answers
  another way.

- **3.38.1** — **Three build lists that had stopped matching the tree.** The
  Visual Studio project, which is hand-listed, had never heard of two of the
  files below and failed to link on Windows; the DOS package had no 8.3 name for
  ``monster_speech.txt`` and quit during init, printing nothing; and the
  autotools install list had been missing four data files since M2. All three
  are now compared against the tree by ``scripts/check-build-lists``, which runs
  on every push rather than in the job that happens to break.
- **3.38.0** — **Five monsters that will not hold still.** ``SHAPECHANGER``:
  the chaos shapechanger, the lord of chaos, the unmaker, Nyarlathotep and
  Dworkin are drawn as some other creature each time the display refreshes, and
  about one time in twenty-five as an object. Nothing about them changes but the
  glyph. Zangband nested the check inside the multi-hued draw, so the flag does
  nothing at all without ``ATTR_MULTI`` — all five carry it, which is why that
  never showed.
- **3.37.0** — **Things that burn you back, and things that bounce.**
  ``REFLECTING`` and the three auras, with the ego flags ``REFLECT``,
  ``SH_FIRE`` and ``SH_ELEC`` that are the same mechanisms from the wearer's
  side. Strike something wrapped in fire and it burns you; a bolt aimed at a
  reflector comes back, nine times in ten, to a grid beside whoever fired it.
  And a monster wrapped in fire will not enter water — a movement rule hidden
  inside a damage flag, and one Zangband never got to use for want of a sea.
- **3.36.0** — **A hundred and two monsters decline to be teleported.**
  ``RES_TELE``, which is not the flat immunity it sounds like: a unique that has
  it is unaffected outright, and anything else gets a saving throw on its size.
  *Teleport Other* is no longer a reliable way to remove an inconvenience.
- **3.35.0** — **Eighty-nine monsters have something to say.** ``CAN_SPEAK``:
  they speak in a fight, differently once they have turned to run, and once more
  when they die. And a unique that could talk turns out, one death in ten, to
  have been wanted for something — the bounty is real gold on a curve reaching
  32,000, which the flag report had filed as pure flavour.
- **3.34.0** — **The blood of Amber is a family.** ``AMBERITE`` gives the twelve
  a shared kind, the ``S_AMBERITES`` summon, and a blood curse laid on whoever
  kills one. Until now they were twelve separate uniques with nothing joining
  them, which is what CNT-02 asked them not to be.

M7: races and classes — 25 to 29 August 2026
----------------------------------------------

- **3.33.2** — **The white deer is no longer a healing fountain.** Reported from
  play: a deer followed the character about, healing them to full every few
  paces, forever. The beast did remember — but the memory was kept on the animal,
  and an ordinary deer is not a unique, so the wilderness destroys and re-rolls it
  every time the map around the player is rebuilt. A fresh deer has forgotten, and
  at depth 1 with rarity 4 there is always another one.

  The memory is now kept on the character as well, for ``world:blessing-turns``
  (500 by default, and tunable without a rebuild). While it lasts every beast
  shies away. The Unicorn was never affected — she is unique, so the wilderness
  brings her back with her memory intact.
- **3.33.1** — **The open sea, and the manual catching up with it.** The first
  pass at populating the water only filled the shallows: deep water is damaging
  terrain and the generator refused it outright, so the ocean proper stayed
  empty. Deep water now names ``IM_WATER`` as the flag a creature needs to be in
  it — exactly as lava names ``IM_FIRE`` — so fish live there and everything else
  still keeps out. The player is unaffected; their drowning was always decided by
  what they carry.

  :doc:`wilderness` said *"nothing else can swim yet"*, which had stopped being
  true, and :doc:`monsters` said nothing about the aquatic monsters at all. Both
  now describe what lives in the sea, that it is never tame however orderly the
  coast, and that the waterline is a line in both directions — a shark cannot
  follow you onto the beach.
- **3.33.0** — **There are fish in the sea.** Reported from play: walking the
  coast turned up nothing. It turned out the sea held nothing whatever — not just
  no fish, no monsters at all. ``wild_populate()`` asked ``square_isempty()`` of
  each grid, that asks for a floor, and neither depth of water carries the FLOOR
  flag, so every square of ocean was refused. The wilderness now populates in two
  passes, land and water, each drawing from its own kind.

  Three things had to be true for it to feel right. The sea has **a density of
  its own**, because the population parameter measures what the *land* supports
  and left open water the emptiest place in the world. It has **a danger floor**,
  because danger derives from how well the country is policed and nobody polices
  the sea — a calm bay off a lawful city had nothing in it, the shallowest fish
  being a swordfish at depth eight. And **fish cannot leave the water**, whether
  by swimming ashore or by being scattered there as a shoal, so a character who
  stays on the sand is in no danger at all.
- **3.32.1** — **Sharks are no longer trees.** Zangband drew its aquatic
  monsters with ``l`` and Angband draws trees with ``l``, so the import gave
  every fish, shark, whale, squid and kraken in the game the tree's base — and a
  base's flags are inherited. All twenty-four were **immune to fear, immune to
  confusion, and regenerating**, and because dungeon dwellers are matched by base,
  **Arden and the Grove of the Unicorn were spawning great white sharks** while
  the Forest monster pit could fill with krakens. The bestiary filed whales under
  *Trees/Ents*.

  There is now a **Fish/Aquatic** base — glyph ``N``, the last free letter in the
  game — and the twenty-four have moved to it. The ents stayed where they were.
  **Faiella-Bionin**, the stairway beneath the sea, and **Rebma**, the drowned
  city, have fish in them for the first time.

  They lose their tiles in the process: they had been borrowing tree pictures,
  and there is no aquatic monster in Angband to borrow from, so they render as
  text until somebody draws them.
- **3.32.0** — **The imported monsters have tiles.** All 391 monsters ZangbandTK
  adds were missing from every one of the five tilesets, so in graphics mode a
  chaos hound, a serpent man and a death sword all rendered as bare letters among
  the pictures. 235 to 256 of them per tileset now borrow the tile of an Angband
  monster sharing their kind, colour and depth — generated by
  ``scripts/tiles/borrow-tiles.py`` and verified by its ``--check`` mode against
  the two ways this fails silently.

  **The 122 imported uniques are deliberately left as text.** Matched by kind and
  depth, Oberon of Amber, Dworkin and Mandor all come out wearing the Mouth of
  Sauron's face. For a game about getting away from that, three Amber princes in
  Tolkien's clothing is worse than no picture: a letter says *not drawn yet*, a
  wrong portrait says something false.
- **3.31.4** — **Four crashes left behind by the tileset filtering.** Dropping
  uninstalled tilesets from the list made ``get_graphics_mode()`` return nothing
  for an id that used to resolve — which is the point — but four places had never
  had to consider that. ``reset_visuals()``, shared by every front end, asserted
  on the result and then dereferenced it, and an assertion is no check at all
  once ``NDEBUG`` is set. In the SDL front end, ``sdl_BuildTileset()``
  dereferenced it outright; the saved configuration was read straight into
  ``use_graphics`` with nothing checking the tileset existed; and the loop that
  binds the current mode matched a tileset by its **position in the list** rather
  than its id, so a missing low-numbered tileset silently selected the wrong one.

  The Cocoa and Windows front ends were already safe, and were left alone.
- **3.31.3** — **A review of M7, and eleven fixes.** Five of the findings were
  things the game did silently and wrongly: a **Yeek's scream and a Sprite's
  sleeping dust did nothing at all** — projected with no dice, so a power of
  zero, paid for in full; a **patron's** *destruction* levelled one grid instead
  of fifteen; a **Draconian's breath** had its arc written into the radius slot,
  making it a needle-thin cone at a fifth of its range; and two of the
  **Mindcrafter's** level bands were identical, because a ball with radius zero
  is given a radius of two.

  The worst was structural: **a patron's reward was handed out from inside the
  level-up loop**, and two rungs on every ladder grant or drain experience — both
  of which call back into that loop. A single kill worth several levels could
  recurse. It now waits until the level has settled, which is what Zangband did
  and why.

  Also: a patron with no reward list crashed during loading rather than
  reporting a parse error; patron messages are now validated as format strings
  the way martial-arts messages already were; *Learn all monsters* skipped the
  first monster in the bestiary; the two new class abilities had no descriptions,
  so a Monk was never told it fights unarmed and a Chaos-Warrior was never told
  it was owned; and the version-1 savefile reader was a verbatim copy of the
  version-2 one, which is now one body with a flag.
- **3.31.2** — **A picture of the coast**, and the machinery to take it. The
  capture harness can now stand a character on a waterline before the first level
  is built, which is where it has to happen: asking for a new level mid-turn is
  honoured by the full game loop and ignored by the headless front end the
  pictures are captured through. Adds a **Learn all monsters** debug command
  (``B``), the companion to *Learn object kinds* that 4.2 has and this did not.
- **3.31.1** — **A Mindcrafter has mana again.** It never had any: the armour
  allowance before mana starts draining is read out of a class's magic block, a
  Mindcrafter has no magic block, and its own starting armour then cancelled more
  mana than the class had. It began the game unable to pay for anything and
  stayed that way for fifty levels, paying for every power in blood. The sidebar
  compounded it by refusing to show spell points to a class without spellbooks,
  so there was nothing on screen to say what was wrong. Both fixed, and *has
  mana* is now one question asked in one place rather than three sites guessing
  from spellbooks. Found from a player's screenshot.

  The manual also gains a **third screenshot**: a Sprite Mindcrafter's power
  list, showing what a character can do that came out of neither a book nor a
  shop.
- **3.31.0** — **The Chaos-Warrior, and nine Lords of Chaos** (PLR-03/PLR-05).
  The twelfth class, and the only one that belongs to somebody. Sworn at birth to
  a Lord it did not choose and cannot leave, and every level it gains the Lord
  decides how it feels: a full healing, a raised stat, experience, an object out
  of nothing — or a cursed weapon, a ruined stat, monsters dropped on top of you,
  or the level destroyed around you. Your Lord is on the character sheet in red,
  because it is not a credential.

  **Level 13 is three times as dangerous as any other**, and nothing in the game
  says so. Every other thirteenth level is twice as dangerous; every fourteenth is
  the safest a Chaos-Warrior ever gets.

  **The nine Lords are Zelazny's.** Zangband's sixteen were Moorcock's Elric gods
  plus Khorne, Slaanesh, Nurgle and Tzeentch from Warhammer, and not one had any
  connection to Amber — the clearest case of drift in the game. They are replaced
  by Swayvill, Suhuy, Mandor, Dara, Gramble, Jurt, Despil, Borel and Gilva, four
  of whom already walk around in the bestiary. See DEC-38.

  Polymorph uses the game's own shapechanges rather than Zangband's version,
  which permanently mangled the character; and one reward in six was a mutation,
  which is left as a deliberate gap until M8.
- **3.30.0** — **The Mindcrafter, and psionics** (PLR-03/PLR-06). The eleventh
  class. Twelve powers that arrive by level rather than out of a book, on ``N``
  alongside anything the character's race can do — from Neural Blast at level 1
  to a telekinetic wave at 28. Deliberately not a realm of magic: nothing to
  study, nothing to choose at birth, nothing that can be taken away.

  Most of the twelve **grow into something else** as the character does, which
  needed a mechanism Angband has no equivalent of. *See what is coming* spots
  monsters at level 2, finds traps and doors at 5, sees invisibility at 15, maps
  the level at 20, grants telepathy from 25, detects everything at 30 and lights
  the whole level at 45 — one power, one key, for an entire career. Racial powers
  gained the same ability, which is where a Draconian's breath changing with
  level can now go.

  And **psionic force**, a kind of damage the game did not have. Every damaging
  type Angband has is an element resisted by a flag or by armour; this one asks
  whether there is a mind there to hurt. A golem, a mould or an animated weapon
  is completely unaffected however hard it is hit; a strange or feeble mind takes
  a third. Against anything else nothing resists it at all.

  The ``N`` command is now **Use a power** rather than *Use a racial power*, since
  it lists both.
- **3.29.1** — **"What's Different" has a Races and Classes page.** The largest
  visible difference between this game and Angband — twenty races instead of
  eleven, powers a race can use, a class that fights better with nothing in its
  hands — was described in the reference manual and nowhere in the section whose
  whole job is to say what is different. It has a page now. The features page
  lists races, powers and the Monk under what is in the game rather than under
  what is coming, and the four stat and ability tables in :doc:`birth` are
  generated from the data files instead of typed, having silently stopped at
  Angband's eleven races and nine classes.
- **3.29.0** — **The Monk, and martial arts** (PLR-03/PLR-04). The tenth class,
  and the first from Zangband. A Monk fighting bare-handed climbs a ladder of
  seventeen techniques — a punch at level 1, a crushing blow at 48 — throwing two
  strikes a turn at the start and eight by the end, which is more than a Warrior
  gets from any weapon. Some techniques stun; a knee doubles a male opponent up;
  a kick to the ankle leaves anything that walks limping. Against 4.2's own
  handling of an empty weapon slot — one point of damage, no criticals — a Monk
  measures about eight times the damage over a hundred blows.

  Armour is the whole cost. Over ten pounds plus four tenths per level across the
  six armour slots and the strikes halve, the to-hit and damage bonuses go, and
  the armour class paid for empty slots stops. Under it, an empty body slot alone
  is worth three points of armour for every two levels. Zangband's numbers
  throughout; the class mapping from Zangband's table to 4.2's was measured
  against the six classes both games share rather than guessed, and the Monk keeps
  Zangband's experience factor, which makes it the only class in the game that
  costs extra to level.
- **3.28.1** — **Racial powers work without mana.** Powers took their cost from
  spell points only, and the two classes with no spellbooks — Warrior and Monk —
  have a maximum of zero, so a Draconian Warrior could never once breathe. Any
  character who had spent their pool was in the same position. Zangband's rule
  applies: short of mana, the price comes out of hit points instead, and the menu
  says which before you commit. The failure chance was also clamped in the wrong
  order and could be reported above 100 per cent for a poor governing stat.
- **3.28.0** — **Racial powers** (PLR-02). Eight of the nine new races can now do
  something no class teaches, on ``N``: an Amberite shifts into shadow and walks
  the Pattern, a Vampire drinks blood, a Draconian breathes, a Golem turns to
  stone, a Yeek screams, a Sprite throws sleeping dust, a Mindflayer blasts a
  mind, a Half-Titan sizes up what it is looking at. Level, mana, governing stat
  and failure chance are Zangband's own numbers rather than new ones, and a test
  holds them there. Powers draw on spell points, so a Warrior of one of these
  races has a small pool of mana that exists only to feed them; failing spends it,
  being refused does not. The Beastman has no power on purpose — its character
  was mutation, which is not a thing you choose to do.

  Two commands also gained the **roguelike keyset** bindings they should have had:
  the racial power is ``&`` there and the quest log ``%``, because ``N`` and ``J``
  are both running keys in that keyset and the commands were reachable only from
  the ``Enter`` menu. There is no letter left to give them — between the two
  keysets every letter and every usable control key in the game is already taken.
- **3.27.1** — **The nine new races are described**, and the four turned down are
  described too. :doc:`birth` had still been saying there were eleven races; there
  are twenty. Each of the new ones now says what it is and what it costs, with a
  note that the experience figure is not a formality here the way it is in
  Angband. And there is a short section on **Half-Ogre, Half-Giant, Cyclops and
  Dark-Elf**, which are not coming: the first three are Half-Troll with one
  resistance swapped, and the last is a dearer Elf. "It exists in the original" is
  not on its own a reason to carry something, and somebody noticing they are
  missing deserves the reasoning rather than a gap.
- **3.27.0** — M7 begins: **nine new races.** Amberite, Beastman, Yeek,
  Draconian, Mindflayer, Vampire, Golem, Sprite and Half-Titan, bringing the
  total to twenty. Curated rather than imported wholesale — of Zangband's
  twenty-one, four are undead wanting a mechanism the game has not got and
  several are close variants of races already here. The mapping between the two
  games was *measured*, not guessed: ten races appear in both, and against those
  the stats, hit dice, disarm, device, save and stealth transfer verbatim, while
  4.2 turns out to have widened melee and archery by roughly double and flattened
  nearly every race's experience cost to 120. Zangband's costs are kept, which is
  what makes an Amberite dear at 225 and a Half-Titan the most expensive thing
  you can be born as at 255.

M6: quests — 24 to 25 August 2026
-----------------------------------

- **3.26.1** — The features page said the platform was **macOS on Apple
  Silicon**, full stop, which has not been true for some time: the game is built
  and packaged for Windows, Linux, DOS, the Nintendo DS and the 3DS as well, and
  the page is the first thing a visitor reads. It now says so, with the honest
  caveat that only macOS is actually played through. And reporting a job at any
  building that hires is now written down as the design rather than apologised
  for as a shortcut: whoever wanted the work done happens to be drinking in this
  inn too, which beats walking back across the world to be paid.
- **3.26.0** — **All six kinds of work, and the features page catches up.** The
  three that were missing are written: a killing at a named depth of a named
  dungeon (only dungeons you have found, only depths that dungeon reaches), a
  killing that counts only above ground, and fetching a particular thing —
  finished by having it, checked against your pack, so buying it or taking it
  from a chest counts as fetching it. Which kind you are offered depends on what
  the world can supply, and every one falls back to a bounty, which needs nothing
  but a bestiary. The features page had been claiming towns and quests were still
  to come since before either landed; it now describes the game as it is.
- **3.25.0** — **A job can be given up.** Walk into a building that hires while
  you owe it something and it lists what you are carrying and offers to let you
  off. This is the fourth thing the lifecycle had to be able to say and could not:
  M6 asks that a quest can be taken, tracked, completed *and failed*, and until
  now a bounty on something twenty levels too deep held one of your slots until
  the character died — taken, impossible to finish, impossible to be rid of. Every
  carried job is listed, not just the oldest. No penalty beyond the walk back.
- **3.24.2** — **Screenshots, at last, and they cannot go stale.** The
  :doc:`screenshots` page shows the whole world of one game — 129 blocks of sea,
  coast, forest and mountain with the towns and dungeon mouths on it — and the
  surface with a town standing in it. Both are captured from the running game
  rather than drawn: the test front end reports every character, position and
  colour the game puts on screen, and a script replays that into SVG using the
  palette read out of ``z-color.c``. Regenerating them runs the current code, so
  they cannot drift the way a folder of old PNGs does. Two debug commands set the
  scene, and neither does anything a player could not do from ``^A``.
- **3.24.1** — **A quest remembers how many it asked for.** It said *"6 small
  kobold — 0 of 0 done"*, and both halves of that were wrong. Angband never
  stored the target count, because its two quests come from ``quest.txt`` and
  only progress had to be remembered; work taken from a building has nowhere
  else to keep it, so it came back as nought of nought — and could never be
  finished either, since the count starts at one and never reaches zero. Saved
  now, in quests block version 4. A job taken under 3.24.0 cannot be
  reconstructed and is dropped on load with a note rather than left owing
  something impossible. And the plural: ``monster.txt`` carries one only for the
  ninety-six names that need it, so everything else now takes a plain "s".
- **3.24.0** — **Quests you can see, and errands that are not killing.** Two new
  kinds of work: carry word to a named town, and go and look at a place nobody
  here has been. Both are finished by *arriving*, which needed a trigger of its
  own — the existing check only ever sees a monster die, so a delivery that named
  a monster would have been completed by killing one. And ``J`` now shows what
  you have taken on, how far along each job is, and where the travelling ones
  point; until now the only way to find out was to walk back into an inn and ask.
  The quests the game is won by are deliberately not in that list: you are on
  those from birth, and "kill the Serpent of Chaos" at the top of a first-level
  character's list gives away the ending and tells you nothing you can act on.
  The manual gains its :doc:`quests` chapter.
- **3.23.3** — **Roads are two grids wide, not three.** Three was the fix for a
  one-grid road being invisible where it turned, and it worked — but it read as a
  motorway running across the country. Two keeps what the widening was for: a
  corner is a two-by-two block of paving, which is enough to see a turn as a
  turn. Routing is untouched, so the same three blocks in a hundred carry a road;
  there is simply a third less paving on them.
- **3.23.2** — **The magesmith and the recharger did nothing for your money.**
  Both were handed a dice string of ``"0"`` and no subtype. Zero is not a small
  amount, it is nothing: the enchant effect tests its subtype as a set of bit
  flags and zero matches none of them, so the magesmith took the fee and did not
  so much as ask which item — and the recharger asked for the item and then
  worked at strength zero, the worst odds in the game. The magesmith now asks
  whether you want a weapon or a suit of armour before naming its price, and a
  weapon gets both to-hit and to-damage. The recharger works at the strength of
  a scroll of Recharging.
- **3.23.1** — **All forty-four debug commands are written down**, in the manual
  and in the in-game help, by submenu and by key. They were never documented
  anywhere — the manual named the nine submenus and gave five examples, and the
  in-game page did the same, so the only way to find out what ``^A`` could do was
  to open every menu and look. Generated from the source rather than transcribed,
  so it is right. The three ZangbandTK added are marked.
- **3.23.0** — WLD-16d: **somebody will give you work.** Walk into an inn that
  is hiring and you are offered a job before you are offered a bed: kill so many
  of a creature, come back, be paid. Quest-giving is a *property* a building
  carries rather than a building of its own — nothing in that path knows what an
  inn is, and moving it to the magetower is one line. The inn has it because that
  is where people who have been somewhere else are sitting, and because a town
  that has fallen keeps no services, so the work dries up exactly where you would
  expect without a rule saying so. About half the towns in a world are hiring.
  The quest list gained eight slots for work taken this way, kept strictly apart
  from the two quests the game is won by.
- **3.22.1** — **The place line actually appears now.** Third attempt, and the
  first two failed for two different reasons. It was given the lowest priority
  in the sidebar, so it was dropped on any screen shorter than 25 lines (fixed
  at 3.21.2) — and then it still did not show, because the sidebar was already
  full: a 24-line terminal spends the top row on messages and the bottom on the
  status line, leaving twenty-two, and there were twenty-three entries. The
  priority filter passed it and the row counter ran it off the bottom, where it
  was drawn underneath the status line and instantly covered. A blank spacer row
  has been given up to make room.
- **3.22.0** — **Two more cheats, for testing a world you have to walk across.**
  ``=`` then ``x``: *Take hit points* asks a number and sets them there — written
  into the character's hit dice rather than the total, so it survives levelling
  and the next step taken — and *Know every place* puts every town and dungeon
  mouth on the map at once, which turns the magetower into a way of getting
  anywhere. Both are also on the debug menu (``^A``), as ``i`` and ``k``.
  Also: **the manual stops calling this game Angband.** Forty of the forty-seven
  references audited became ZangbandTK. The FAQ's development section is cut —
  it described the *Angband* project's process and pointed anyone who wanted to
  help at the wrong project. And the quick demo no longer tells new players they
  cannot leave the town, which has been the opposite of true since M5.
- **3.21.3** — **No more stranded paving.** A road is routed *to* a town, so its
  last stretch runs across ground the town is then drawn on top of — and the wall
  and gate could leave a grid of paving stranded just outside them, with nothing
  but wall, door and street around it. Nought to three grids across forty-eight
  towns, which is rare and is still a road that goes nowhere. They are taken up
  now rather than joined up, because the town is standing where the rest of that
  road used to be.
- **3.21.2** — **The sidebar says what kind of place you are in, and whether it
  has fallen.** Under the name: ``village``, ``town``, ``city``, ``great city``
  — and in red, ``city, taken`` for one held by monsters, ``town, wild`` for one
  the animals have taken back, ``village, empty`` for one that simply stands
  there. A fallen town keeps its shops but no services at all, so it reads as a
  large well-supplied city with no inn, no healer and no magetower and nothing
  said why; the only clue was its name coming from the lawless list, which no
  player should have to know. Reported twice from play, once as "there is no mage
  tower in this town" and once as "not many stores in this great city, no inn, no
  healer" — both correct behaviour, both invisible. The line was added at 3.21.0
  and never actually appeared: the sidebar drops rows whose priority exceeds the
  terminal height less two, and it had been given the lowest priority of all.
- **3.21.1** — **Your world is your world again.** Adding the magic fractal at
  3.14.0 put one more draw into the middle of the shared world-seed stream, which
  shifted every draw after it — so every existing character's world was quietly
  replaced: different rivers, different towns in different places with different
  names. The world is never saved (it regenerates from the seed) and what you know
  of it is stored by *name*, so a character who had walked to a great city called
  Helgram was loaded into a world with no such place, and the visit was discarded.
  That is why the magetower took your money and carried you nowhere: the
  destination it offered had ceased to exist. Magic now draws from a stream of its
  own and the shared one is left exactly as it was, so worlds made before 3.14.0
  come back the way they were. Characters made *between* 3.14.0 and here will
  shift once more, back to their original world.
- **3.21.0** — **A road out of a gate goes somewhere.** Reported from play: the
  road leaves the town and just stops. It did. The approach paved three grids out
  of every gate and *then* went looking for the road network to join, so every
  gate the network did not reach kept a stub pointing into open country —
  measured, 147 of 508 gates over six worlds, better than one in four. A town has
  four gates and the roads commonly reach one or two, so this was never rare, and
  it is worse than having no road at all: a road is a promise that it goes
  somewhere. Nothing is paved now until the join is found.
  Also, **the sidebar says what kind of place you are in** — village, town, city
  or great city, under the name. How big a place is says what is in it: a village
  keeps three or four trades and no services at all unless it is the one you
  started in. The name alone said none of that, so standing in a village
  wondering where its magetower was is a question the screen can now answer.
- **3.20.0** — **The game ends in Amber now.** Angband is won by killing Sauron
  and then Morgoth; ZangbandTK is won by killing Oberon and then the Serpent of
  Chaos, at the bottom of the Courts of Chaos. That is Zangband's own
  replacement, and it was already the right one — the Serpent is Zelazny's, and
  the bestiary's description of it has been sitting there all along: *"The Unicorn
  of Order fought with Serpent and stole one of its eyes, known as the Jewel of
  Judgement. With the Jewel, Dworkin drew the Pattern."* The Unicorn went in
  yesterday; this is the other half of her. Quests now also name the dungeon they
  are in, because a depth no longer names a place: the Courts run 75 to 110 and
  the Abyss 90 to 127, so depth 100 is two different places and the ending would
  otherwise have been completable in the wrong one.
- **3.19.0** — M6 begins: **quests have a lifecycle.** Untaken, taken, complete,
  finished — four states, saved with the character. Angband has two quests, both
  alive from birth, and records one as done by zeroing the depth it lives at;
  that says everything about a quest which can only be finished and nothing about
  one you accept, carry around, and hand back. Nothing is player-visible yet: this
  is the floor the rest of M6 stands on. One existing rule did change, though —
  winning now counts only the quests the game ends on, because "you have no
  outstanding quests" stops meaning "you have finished the game" the moment a
  townsman can give you an errand.

M5: towns, roads and dungeons — 19 to 24 August 2026
------------------------------------------------------

- **3.18.0** — **The Unicorn of Amber.** Silver-white, watching you with an
  interest you have not earned, and always about to leave — there is one of her
  in the world. Walk into her and she makes you whole: not your wounds only, but
  everything the town healer sells and charges for, at once and for nothing.
  Once, and then she is gone about her business. She carries the same flag the
  white deer does; being *unique* is what makes the blessing the greater one,
  which is the difference itself rather than a second mechanic. The first content
  here written from the Amber novels rather than filtered out of Zangband.
- **3.17.0** — CNT-20: **not everything is there to be fought.** A white deer
  stands too still and too unafraid of you; walk into it and it heals you —
  everything, whatever you were down to — then bounds away ten grids and carries
  on grazing. Once per deer, and the deer remembers, savefile included, so
  following it about does not work. It will not fight back, carries nothing and
  is worth no experience. They are uncommon and live in quiet country, so finding
  one is luck rather than a plan. Neither Angband's nor Zangband's — it goes in a
  third bestiary file so its provenance stays obvious.
  Also: the in-game manual gains a **Towns and services** page, which is where the
  inn's dreams, the shop quality ladder and the one-house rule now live properly
  rather than crammed into the symbol reference.
- **3.16.0** — PLR-41: **the inn's dream**, which closes the last open item from
  DEC-32. Zangband's inn carried a nightmare that reached the sanity blast we
  dropped; the dream survives it. Sleep and you may see a true dream, which puts
  the nearest place you have not found onto the world map — it tells you where to
  walk and does not carry you there, because the magetower goes only to places
  you have actually stood in. Or you may have a dark dream, and be hunted through
  your sleep by something you have met: hold your nerve and you merely remember
  it, fail and you wake frightened or confused. Which you get depends on the law
  of the town, so a well-ordered city is a good place to sleep and a frontier town
  is not. The constraint from DEC-32 held: no insanity, no amnesia, no mutation
  trigger. It is also the exact mirror of the lotus — that takes places off your
  map, this puts one on.
- **3.15.0** — PLR-40, and the first thing in this project that is not a port of
  anything: **the lotus**. A mushroom that looks like any other until you have
  eaten one, and five turns after you do, you forget everything — the map under
  your feet, the world map and every place on it, every monster you had learned,
  what all your potions are, and every spell you knew. It takes nothing you
  cannot earn back and nothing comes back quickly, so the cost is hours rather
  than a character. You always still know where home is, because the magetower
  only travels to places you have found and a character who had forgotten
  everywhere would have nowhere to go. The idea is Zelazny's: the first Amber
  novel opens on a man with no memory who knows only that there is a place called
  Amber and that he belongs to it. Also fixes a test that failed about one run in
  five for a correct reason — it walked the window and asserted the other axis
  held still, which is only true if the character did not start near the window's
  edge, and block alignment decides that.
- **3.14.1** — The manual now says that there is only one house. Every town keeps
  a home and they are all the same one, so what you leave in your village is on
  the shelf when you open the door in a city four days' walk away. Asked from
  play, which is fair warning that the machinery reads the other way: a *shop* in
  another town is another shop and is stocked afresh. A house per town would
  strand your spare gear wherever you happened to be standing when you outgrew
  it, so the house travels with you and the only cost of leaving is the walk back.
- **3.14.0** — WLD-15 and WLD-16a: shops come at a standard as well as a trade.
  A plain *Weapon Smiths*, or an *Advanced*, *Expert* or *Arcane* one, written
  above the door. A better shop draws its goods from the deeper end of what that
  trade sells, puts better magic on them — about three times the plusses at the
  top rung — and keeps a fuller shelf. Which rung a town's shop gets is scored
  from the country around it, and mostly from magic, which is a new fourth
  parameter on the world map: population and law were already there, but towns
  are *sited* on law, so law had nothing left to say. Seven shops in ten are
  plain and one in fifty is arcane. Zangband hand-authored 113 building types to
  get this, about 73 of them rungs on a ladder; here it is three records in
  ``quality.txt`` and a score. One thing you may notice as a side effect: a
  trade's shelves are restocked when you carry your custom to a different town,
  so towns no longer all show the same stock. Walking out and back in does not
  re-roll it.
- **3.13.2** — Roads are three grids wide. One grid was invisible where a road
  turned a corner in the block you were standing in, and a road that turns read
  as a road that ends — reported from play as a road that "appears to end at the
  beach" after a long walk. Corners are squared off so a turn looks like a turn.
  Also: a service building could be promised and then demolished. The generator
  places services on lots off the streets and the ruin pass that follows skips
  lots that already have a building — but it only recognised shops, so a ruin
  could be built on top of a freshly built magetower. That is what made the
  magetower go missing from towns that said they had one: about one service in
  ten, and one in six in a village. Services also now have a street cut up to
  them, which previously only shops did.
- **3.13.1** — The healer's "restore your memory" is now "restore your lost
  levels", which is what it actually does — Angband calls it life force, and
  memory was another game's idiom. The manual explains what drains it and what
  losing it costs you.
- **3.13.0** — WLD-16c finished: the inn, the healer, the magesmith and the
  recharger join the magetower. A town has an inn, a healer and a magetower; a
  city adds the magesmith and the recharger; a town that has fallen has none of
  them. All of them charge, with the prices in ``constants.txt``. The inn is the
  one that earns its keep here more than it would in Angband — daylight is what
  reveals the overworld, so a bed until morning beats waiting a hundred turns at
  a time.
- **3.12.2** — The magetower's menu had to be dismissed once for every shop you
  had visited that game. Its handler was registered where the game re-registers
  it on leaving a store, and nothing removes handlers before adding them, so they
  stacked.
- **3.12.1** — Every town above a village keeps a magetower, unless it has
  fallen. It was scored on population and law before, and the scoring turned out
  to put every *town* just under the threshold — so only cities had one, and
  nothing said so. Eleven of twelve towns on a measured world now have one.
- **3.12.0** — The status line says ``Day`` or ``Night`` above ground. Angband
  never said, because its surface is one town-sized level; here daylight is what
  reveals the country, and at night you see as far as your lamp — which is
  indistinguishable from a broken map if nothing tells you the hour. Both cheats
  and the debug commands are now documented, in the manual and in the in-game
  help as page (e).
- **3.11.2** — Your home village always has a magetower now: travel was
  unusable from the one place every journey starts. The status line also refreshes
  the place name as you walk out of a town, where before it kept showing the town
  you had left. And the gold cheat is in the cheat options screen (``=`` then
  ``x``) as well as the debug menu.
- **3.11.1** — Three things found by playing. Roads reached towns but often
  stopped at a blank wall with the gate up to twenty-six grids along it, which is
  a dead end from where you are standing; the gate is now cut where the road
  arrives, and the last stretch of road is drawn up to it. The magetower did not
  offer your own village, because the visited flag was only set by taking a step
  and your steps were taken before the flag existed. And ``^A`` ``$`` puts gold
  in your pocket, for testing things that cost money.
- **3.11.0** — The magetower (WLD-16c), and with it the mechanism services are
  built on (WLD-18): a building is a door with behaviour behind it. Walk into a
  tower and the mages will carry you to a town you have stood in or a dungeon
  mouth you have seen, for a fare that rises with the distance. About a third of
  towns keep one — never a village, never a town that has fallen. The savefile's
  wilderness block goes to version 5; all twenty existing saves still load.
- **3.10.0** — Towns have names, and the status line shows the one you are
  standing in. Where Angband said ``Town`` at depth zero — which here meant the
  whole world, market square and empty moorland alike — it now names the place, or
  says ``wilderness``. A place in governed country and a place that has fallen
  draw their names from different lists, so what somewhere is called tells you
  something before you arrive.
- **3.9.0** — Towns differ in who lives in them (WLD-11): about three in five
  have people, one in four has been emptied and taken back by animals, one in ten
  is held by monsters, and one in twenty stands empty with its shops intact. Law
  decides it, and a large town holds out longer than a hamlet. Your home village
  always has people in it. The download page also caught up with the fact that
  there are Windows, Linux and Nintendo builds now, not just a macOS image.
- **3.8.0** — Every dungeon mouth now has a road to its door. Six of the thirteen
  happened to sit on one; the rest were between eleven and sixty-two blocks away,
  which is up to a thousand grids of open country to search with nothing to
  follow. Better siting could not fix it — the deep dungeons belong a long way
  from any town — so the road goes to them instead.
- **3.7.1** — A code review of the whole milestone, and it found that **eleven of
  the thirteen dungeons could not be entered**: the depth clamp treated "above
  the top of this dungeon" as one case when it is two, so stepping onto the mouth
  of the Abyss asked for depth 1, found it above the top of 90, and returned the
  surface. Also: a dungeon's inhabitants followed the player home into the open
  country, the object theme was defeating Angband's rejection of unreadable
  spellbooks, and the fix for walls behind trees only worked when touching them.
  Fifteen findings in all.
- **3.7.0** — Each dungeon is now home to its own kinds of creature (CNT-05).
  The Caverns of Kolvir run to trolls and giants, Tir-na Nog'th to wraiths and
  vortices, Arden to animals and trees. A stranger from elsewhere turns up now
  and then, at rather less than a fifth of what is met.
- **3.6.0** — Each dungeon yields its own kind of treasure (CNT-12). Rebma is
  rich, Garnath is not, the Grove of the Unicorn runs to magic and the Caverns of
  Kolvir to tools. What a dungeon yields changes; how much does not.
- **3.5.1** — The in-game manual learned the dungeons: a page of its own listing
  all thirteen with their depths, the rule that a dungeon ends at its bottom, and
  how recall behaves per dungeon. The symbol list gained the ``>`` of a dungeon
  mouth out in the open.
- **3.5.0** — Thirteen dungeons (WLD-14), Amber's own places, each covering a
  range of depths and ending at the bottom of it. Each remembers how far down it
  has been explored, so recall returns you to the dungeon you were last in. Each
  has its own floor and its own preferred layout. The savefile's wilderness block
  goes to version 4; older saves still load.
- **3.4.1** — A town wall you were standing next to was invisible if you were
  standing in trees. Angband lights a wall only if the grid between it and you
  carries light onto its face, which assumes anything blocking sight is a wall
  nobody can stand in; ZangbandTK's trees are passable and block sight, so the
  grid you occupied was judged to block the light. Only the stretches of wall
  with grass in front of them lit up, which is what made it puzzling.
- **3.4.0** — What you know of the surface now survives a save made while you are
  underground, not just a dungeon trip within one session. The savefile's
  wilderness block goes to version 3; older saves still load.
- **3.3.1** — The in-game help caught up with the game: it points at
  zangbandtk.com rather than Angband's manual, ``M`` is described as showing the
  world map out of doors, and the symbol list learned the overworld — grass,
  trees, water, mountainside, road — along with a note that not every town holds
  all eight shops.
- Also in 3.3.1: the overworld window scrolls rather than jumping the view
  across. A rebuild re-anchored both axes when only one had asked for it, and
  threw the panel away and chose it afresh, so a long walk west appeared to drop
  the character a dozen rows they had never walked.
- **3.3.0** — Roads (WLD-08). The towns are joined by routed roads that follow
  the valleys and go round the mountains; every town is reachable from the
  starting village along them. The overworld map is also remembered across a trip
  to the dungeon, which it was not before: coming back up put you in a town you
  had to learn again.
- **3.2.1** — Three faults that made a town away from home unusable. Larger towns
  came out as empty fields inside their walls, because every building lot was
  clamped to the starting village's size; no town was drawn at all once the window
  no longer covered home; and about one town in fifty lost its down staircase to
  the gate-cutting.
- **3.2.0** — A dozen places in four sizes (WLD-10, WLD-11, WLD-11a, WLD-12),
  keeping different trades according to the country around them. The starting
  village became the smallest place in the world, deliberately: see DEC-29.
- Releases gained a **Windows zip** beside the disk image: a 32-bit build,
  packaged with the manual and a ``README.txt`` for SmartScreen. Windows had been
  dropped from the release as untested; the mingw cross build, MSBuild, MSYS2 and
  Cygwin all pass now.

3.1.1 — the first release, 18 August 2026
=========================================

Tagged at the end of M5's first day, so everything below it shipped in it: M0 to
M4 complete, and M5 begun. CI builds the release from the tag.

M5 begins: the world map and the front door — 18 August 2026
------------------------------------------------------------

- **3.1.1** — Four faults: the Visual Studio project had never been told about
  the wilderness source, so the one build system that lists its sources by hand
  would not link; no savefile would open, because three additions to the
  wilderness block had all been left at version 1, so its shape could not be told
  from its version; and the world map and the menu each had one of their own.
- **3.1.0** — An overhead map of the world (WLD-25). ``M`` out of doors draws the
  whole world, one character to a block, from the same table the ground is drawn
  from, so what the map calls forest is what you walk into. It pans rather than
  scales, because squeezing 129 rows into 22 would lose the coastlines. In the
  dungeon ``M`` is still the level map.
- **3.0.0** — The game's own name and version on the title screen and in the
  About panel, in place of Angband's and in place of the ``git describe`` string
  the build systems had been handing it, and an icon of its own, cleaned up from
  Zangband's.
- **The town has gates.** The boundary is sealed and four gateways are cut, one
  per side, each two tiles wide with doors that swing shut, in place of the ragged
  holes Angband's starburst clearing happened to leave.
- **Releases are cut from a tag.** CI builds a disk image whose bundle is signed
  ad-hoc and verifies that signature before publishing, with the source archive
  beside it. Ad-hoc signing is not notarization, so the first launch has to be
  allowed by hand once — :doc:`download` says how.
- Fixes: the 14 issues a code review turned up before M5 started, and a window
  scroll that moved the player with it.
- The players' guide became this game's rather than Angband's, and the unused
  Read the Docs configuration went.

Documentation and site — 17 August 2026
---------------------------------------

- The manual became ZangbandTK's rather than Angband's, and moved off
  sphinx-better-theme (unmaintained since 2013) onto pydata-sphinx-theme.
- The site is published to `zangbandtk.com <https://zangbandtk.com/>`_ from the
  Sphinx sources on every push, replacing a documentation build that was never
  deployed anywhere.
- The manual's chapters were grouped rather than left as 32 top-level entries.
- Renaming was scoped to the site's identity. Angband is still named throughout
  the manual body where the reference is correct, and the credits and licence
  pages are unchanged.

M4: the wilderness — 16 to 17 August 2026
-----------------------------------------

Zangband's defining feature, and the largest piece of work so far.

- **The world map**: fractal terrain over a parameter space, 2064 grids square,
  generated from a seed and never stored.
- **One continuous surface**, not a set of levels, scrolling as the player walks.
- **The town stands in the world**, with roads out of it, and you can walk out.
- **Rivers run to the sea and lakes sit in the hollows** (WLD-08).
- **Deep water is crossed, not walled off** — it can be waded and drowned in —
  and the world is not known in advance.
- **The world ends in sea**, not in a wall.
- **The wilderness is inhabited**, and the world is Zangband-sized (CNT-05).
- **The world remembers what you left, and forgets it in time** (WLD-04). A
  unique you wounded and left is still out there (WLD-04b).
- Zangband's own wilderness glyphs, rather than Angband's.
- Fixes: the crash on quit, the missing townspeople, and 4.2's town rock kept.
  Farmer Maggot turned up three faults, one of them serious.
- WLD-05 and WLD-06 were withdrawn, along with the code WLD-05 had asked for.

M3: curses and vaults — 15 August 2026
--------------------------------------

- **The Ancient and Foul Curse**, with its cascade intact, and random object
  abilities.
- Pit themes and vaults — including a finding about Zangband's vaults.

M2: the bestiary and the loot — 15 August 2026
----------------------------------------------

- **389 monsters** imported from Zangband, including the princes of Amber and the
  Mythos deities, bringing the total to 1013.
- **51 artifacts**, all of them, including Grayswandir and Frakir.
- **18 ego types**, all of them, including ``(Vampiric)``, ``(Chaotic)`` and
  ``(Trump Weapon)``.
- **Vampiric, vorpal and chaotic weapons** — three mechanics Angband has no
  equivalent of.
- Manual chapters for monsters and objects.

M1: lethality — 15 August 2026
------------------------------

- **The lethality scalar**: every monster at 73% of Angband's hit points and 50%
  of its armour class, the measured difference between Zangband 2.7.5 and the
  Angband it forked from. This was the first build that played as something other
  than Angband.

M0: foundations — 15 August 2026
--------------------------------

- Project identity, the ``tools/zconv`` data conversion tooling, and the manual
  scaffolding.
- The project was renamed to ZangbandTK on 16 August 2026.
