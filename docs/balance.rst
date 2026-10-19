How Balance Differs
===================

ZangbandTK is built on Angband 4.2 and rebuilds the character of Zangband, a
variant whose development stopped in 2005. It doesn't match either of them
exactly. If you've played one of them, these are the differences to expect.

Monsters die sooner
-------------------

Every monster in ZangbandTK has 73% of Angband's hit points and 50% of its
armour class.

That one change is what makes the game feel different, and it's the only thing
that makes ZangbandTK faster than vanilla Angband. Monsters die sooner and are
easier to hit, so fights are over quicker. They also kill you sooner: a fight
you'd have survived in Angband by grinding it out is now decided in fewer
turns, whichever way it was going.

The figures come from Zangband itself. Across the 450 monsters that Zangband
2.7.5 and Angband 2.8.1 have in common, Zangband's monsters had a median 0.73×
the hit points and 0.50× the armour class of the Angband release it forked
from.

Why scale rather than import
----------------------------

ZangbandTK doesn't use Zangband's own per-monster numbers for creatures that
already exist in Angband. It scales Angband 4.2's numbers instead.

Angband spent twenty-five years tuning how its monsters compare with each
other: which ones are dangerous for their depth, which are pushovers, which
punch above their weight. Importing Zangband's 2005 values wholesale would have
thrown all of that away for nothing, since those relationships aren't what
makes Zangband distinctive. Scaling keeps Angband's relative balance and takes
Zangband's absolute lethality.

Monsters that exist only in Zangband have no Angband version to scale, so
they're placed on Angband 4.2's own depth curve. Zangband's numbers are only
used as a hint to the creature's role: whether it's meant to be tanky, fragile
or fast for its depth.

Experience is unchanged
-----------------------

ZangbandTK awards experience exactly as Angband 4.2 does. There's no
multiplier, and the levelling thresholds are untouched.

This surprises people who remember Zangband as a faster game. At first glance
Zangband seems to award about twenty times more experience per monster, but
that's down to how the two games store the value. Angband stores experience
*per monster level* and awards ``value × level ÷ player level``; Zangband
stored the *total* and awarded ``value ÷ player level``. Allow for that and
Zangband's experience per kill is within 2% of Angband's, a median ratio of
0.98 across the monsters they share.

Zangband's levelling threshold table is byte-identical to Angband's, and its
race experience factors are slightly *higher*, which means slightly slower
levelling. So there's no faster progression to inherit. What made Zangband
feel quick was monsters dying sooner, which is the change described above.

Tuning it yourself
------------------

Both values live in ``lib/gamedata/constants.txt`` and take effect when you
restart. No rebuild is needed::

    lethality:hit-points:73
    lethality:armor-class:50

They're percentages of each monster's base value. Set both to 100 and the game
behaves exactly like vanilla Angband 4.2. That's a supported setup, useful for
comparison, and for players who want ZangbandTK's content at Angband's pace.

Lower values make the game faster and more lethal in both directions. There's
no upper limit. Hit points floor at 1 and armour class at 0, though, so very
low percentages squash the difference between weak and strong monsters.

Savefiles
---------

ZangbandTK doesn't load Angband or Zangband savefiles, and never will. The
player model, level persistence and world layout have moved too far apart for
a converted character to mean anything. Savefiles are tagged ``ZZK1``, and the
game won't open anything else, not even partly.
