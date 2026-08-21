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
