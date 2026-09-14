How Balance Differs
===================

ZangbandTK is built on Angband 4.2 and rebuilds the character of Zangband, a
variant whose development stopped in 2005. It matches neither of its ancestors
exactly. This chapter states plainly how it differs from both, so that players
arriving from either know what to expect.

Monsters die sooner
-------------------

Every monster in ZangbandTK carries **73% of Angband's hit points** and **50% of
its armour class**.

This is the single change that makes the game feel different, and it is the only
mechanism by which ZangbandTK moves faster than vanilla Angband. Monsters die
sooner and are easier to hit, so encounters resolve faster and turn over more
quickly. They also kill you sooner: a fight you would have survived in Angband
by grinding it out is now decided in fewer turns, in whichever direction it was
going.

Those figures are not invented. They are what Zangband did, measured across the
450 monsters that Zangband 2.7.5 and Angband 2.8.1 have in common — Zangband's
monsters carried a median 0.73× the hit points and 0.50× the armour class of the
Angband release it forked from.

Why scale rather than import
----------------------------

ZangbandTK does not use Zangband's own per-monster numbers for creatures that
already exist in Angband. It scales Angband 4.2's numbers instead.

Angband spent twenty-five years tuning the relationships *between* its monsters
— which are dangerous for their depth, which are pushovers, which punch above
their weight. Importing Zangband's 2005 values wholesale would have discarded
all of that to no benefit, because those relationships are not what makes
Zangband distinctive. Scaling keeps Angband's relative balance and adopts
Zangband's absolute lethality.

Monsters that exist only in Zangband have no Angband counterpart to scale, so
they are placed on Angband 4.2's own depth curve, using Zangband's numbers only
as a signal of their relative role — whether a creature is meant to be tanky,
fragile or fast for its depth.

Experience is unchanged
-----------------------

ZangbandTK awards experience exactly as Angband 4.2 does. There is no
multiplier, and levelling thresholds are untouched.

This surprises people who remember Zangband as a faster game, so it is worth
stating why. Zangband appears at first glance to award roughly twenty times more
experience per monster, but this is an artefact of how the two games store the
value. Angband stores experience *per monster level* and awards
``value × level ÷ player level``; Zangband stored the *total* and awarded
``value ÷ player level``. Once that difference is accounted for, Zangband's
experience per kill is within 2% of Angband's — a median ratio of 0.98 across
the monsters they share.

Zangband's levelling threshold table is byte-identical to Angband's, and its
race experience factors are slightly *higher*, meaning marginally slower
levelling. There is simply no faster progression to inherit. What made Zangband
feel quick was monsters dying sooner, which is the change described above.

Tuning it yourself
------------------

Both scalars live in ``lib/gamedata/constants.txt`` and take effect on restart,
with no rebuild required::

    lethality:hit-points:73
    lethality:armor-class:50

They are percentages of each monster's base value. **Setting both to 100 gives
behaviour identical to vanilla Angband 4.2**, which is a supported
configuration — useful for comparison, and for players who want ZangbandTK's
content at Angband's pacing.

Lower values make the game faster and more lethal in both directions. There is
no upper limit, but note that hit points floor at 1 and armour class at 0, so
very low percentages compress the difference between weak and strong monsters.

Savefiles
---------

ZangbandTK does not load Angband or Zangband savefiles, and never will. The
player model, level persistence and world layout all diverge too far for a
converted character to be meaningful. Savefiles carry the tag ``ZZK1``, and the
game will decline to open anything else rather than load it partially.
