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
for M9. **M8, mutations and virtues, is next.** See :doc:`features` for what that
adds up to in the game, and for what the rest will bring.

Version numbers move with the work — patch for a fix, minor for a feature,
bumped in the commit that does it — so a build can be identified from its title
bar. They begin at 3.0.0, continuing Zangband's own line from 2.7.5-pre1 rather
than the Angband 4.2.6 the code sits on.


Unreleased
==========

Savefile compatibility — 30 August 2026
----------------------------------------

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
