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
