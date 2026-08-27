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
Warrior could never once breathe. Nine of the fourteen classes were locked out
of a feature I had documented as working for exactly them. Zangband's own answer
turns out to be that a character short of mana pays in hit points instead, which
is both the fix and, I suspect, why Zangband wrote it that way in the first
place.

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
