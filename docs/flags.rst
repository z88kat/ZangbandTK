=========================
Resistances and Abilities
=========================

A lot of what keeps your character alive isn't a number on the character
sheet. It's a property: a resistance to fire, the ability to see invisible
monsters, a curse that stops you teleporting. Your race and class give you a
few of these, the things you wear give you most of them, and potions and spells
give you some for a while.

ZangbandTK lists them all on one page of the Character window, the Flags page.
The text-mode game shows the same information on the second view of the
character sheet, which :doc:`command` describes under Character Description.
The list of properties at the end of this page applies to both.

The Flags page
==============

Open the Character window with ``C`` and choose Flags. Press F1, or the
Help... button on the page, to come back here.

Reading the grid
----------------

Every row is something that can protect you, help you or hold you back. Every
column is somewhere it can come from: the twelve things you can wear, in the
order the Items window shows them, then You (your race, your class and what
you've gained with your levels), then Timed (potions, scrolls and spells,
which wear off).

The Result column, next to each name, is what the row adds up to. Start
there. It answers "am I protected against this?" without you having to read
across the row.

For example:

================  ==============  ======  ======  ======  ======  ======
Property          Result          Weapon  Body    Cloak   Shield  Timed
================  ==============  ======  ======  ======  ======  ======
Fire              ``~`` For now   ``+``   ``+``   ``−``   ``+``   ``~``
Shards            ``+`` Resist    ``·``   ``·``   ``+``   ``·``   ``·``
No teleportation  ``!`` Affected  ``·``   ``·``   ``·``   ``·``   ``·``
================  ==============  ======  ======  ======  ======  ======

Here three items resist fire, but a cursed cloak makes you vulnerable to it,
and a vulnerability cancels a resistance. Only the potion in the Timed column
is protecting you, and only until it wears off.

The rows come in four groups: resistances, abilities, drawbacks and modifiers.
Click a group's band to fold it away, and click it again to open it. Each band
also sums up its group, for instance "6 of 13 resisted, 1 for now, 3 not
known".

Marks
-----

Each mark has its own colour as well, so you can tell them apart in any
palette.

=======  ==================================================================
Mark     Meaning
=======  ==================================================================
``+``    This gives it to you, for as long as you wear it.
``*``    This makes you immune. Resistances only.
``~``    You have it for now, from a potion, scroll or spell. Timed column
         only.
``−``    This works against you: it makes you vulnerable, or takes the
         property away.
``!``    This puts a drawback on you. Drawbacks only.
``?``    Not known. The item has a rune you haven't learned, and this could
         be it.
``·``    You're wearing something here, and it doesn't give this.
(blank)  A grey column is an empty slot. A blank cell means the column
         can't give this at all, such as You on a row only items can have.
=======  ==================================================================

A grey, sloping name means you haven't learned that row's rune yet. Its values
are still shown.

Results
-------

The Result column says, in a word, where each row ends up.

============  ================================================
Group         What the Result can say
============  ================================================
Resistances   Immune, Resist, For now, Unknown, Vulnerable, no
Abilities     Yes, For now, Unknown, Blocked, no
Drawbacks     Affected, For now, Unknown, no
Modifiers     the total, such as ``+7``
============  ================================================

Three rules decide it:

- A resistance and a vulnerability cancel each other out, so one cursed item
  can undo three good ones. An immunity isn't cancelled.
- A timed effect adds one level on top. When that's what makes the difference,
  the Result says For now, and it'll change back when the effect wears off.
- Unknown means nothing you know about gives you the property, but an item
  you're wearing has a rune you haven't learned and might. The Result only ever
  counts what you know, so it can't give away an unidentified property.

Columns
-------

The slot columns are headed by the item's own symbol, in its colour, with the
slot's name under it: Weapon, Bow, Ring R, Ring L, Amulet, Light, Body, Cloak,
Shield, Helm, Gloves and Boots. An empty slot shows the symbol for that kind of
item, dimmed. A small ``?`` on a heading means that item has runes you haven't
learned yet.

The You column is what your race, your class and your levels give you. You can't take
these off. The Timed column is what potions, scrolls and spells are giving you right
now, with the turns left.

To decide what to take off, point at an item's heading. The card that comes up
lists what you'd lose, what you'd gain and what wouldn't change if you took it
off, row by row: "Shards: resist → no", "Fire: for now → resist". If nothing
would change, the card tells you that whatever else you're wearing covers all
of it.

Runes you haven't learned
-------------------------

A property is a rune, and you learn each rune the first time you notice it on
an item, usually by being hit with the thing it protects against. Until then
the row's name is grey, and an item that might have it shows ``?`` in that row.

The game knows that an item has runes you haven't learned, but not which. So
the ``?`` goes in that item's column on every row whose rune you haven't
learned. Putting it on the one row the item really has would give the answer
away.

A new character has learned very few runes, so most of the page starts out
grey. A note above the grid says how many you've learned so far.

Finding out more
----------------

Point at anything on the grid and the status bar at the bottom of the Character
window explains it straight away. Leave the pointer still for a moment and a
card comes up with the whole story:

- On a row's name, its Result or one of its cells, the card says what the
  property does, lists everything that gives it or works against it, and ends
  with the answer. When two or more things give it, it says you could lose one
  and keep the property.
- On a column heading, it names the item and any curse on it, and says what
  you'd lose or gain by taking it off.

The card goes away when you move off the grid, scroll, click or press a key.
Keys still go to the game, so you can leave the window open while you play.

The Show buttons above the grid narrow it down: All; In effect (rows where
the Result isn't no or Unknown); Against you (rows with a ``−`` or ``!``); and
Not yet learned (rows whose rune you haven't learned, or with a ``?``). The
numbers beside them don't change as you switch.

Click a row's name to mark it, or a heading to mark its column, so you can
follow it across or down. Click again to clear it. Double-click a heading to
inspect that item, as the Items window's Inspect does. View ▸ Fold All Groups
and Unfold All Groups fold or open every band at once.

Not on this page: your race's and class's own abilities, which nothing can add
or take away (the ``S`` command lists those); mutations and shapes; and a
weapon's brands, which belong to its blows and are listed on the weapon.

What each property does
=======================

These are the rows of the Flags page, in the order it shows them. The card for
a row quotes its entry here.

Resistances
-----------

Acid
  Acid attacks hurt you less, and are less likely to damage what you carry.

Lightning
  Lightning attacks hurt you less, and are less likely to destroy what you
  carry.

Fire
  Fire attacks hurt you less, and are less likely to burn what you carry.

Cold
  Cold attacks hurt you less, and are less likely to shatter what you carry.

Poison
  Poison attacks hurt you less, and can't leave you poisoned.

Light
  Bright light hurts you less, and can't blind or dazzle you.

Darkness
  Darkness hurts you less, and can't blind you. Unresisted darkness from a
  powerful monster can also drain your experience, slow you and make you
  forget things. Resisting it stops all of that.

Sound
  Sound attacks hurt you less, and can't stun or confuse you.

Shards
  Shard attacks hurt you less, and can't cut you.

Nexus
  Nexus attacks hurt you less, and can't teleport you, pull you to a monster
  or scramble your stats.

Nether
  Nether attacks hurt you less, and can't drain your experience.

Chaos
  Chaos attacks hurt you less, and can't confuse you, make you see things or
  drain your experience.

Disenchantment
  Disenchantment hurts you less, and can't strip the bonuses from your
  equipment.

Abilities
---------

Fear resistance
  You can't be frightened, so you can always fight back.

Blindness resistance
  You can't be blinded, so you can still read scrolls and see what's around
  you.

Confusion resistance
  You can't be confused, so you can still read scrolls and walk where you mean
  to.

Stun resistance
  You can't be stunned or knocked out.

Cut resistance
  You can't be cut, so you never bleed.

Hold life
  Protects your experience from being drained.

Regeneration
  You recover hit points and mana faster, but get hungry faster too.

Telepathy
  You sense the minds of nearby monsters, even through walls. Mindless
  monsters stay hidden.

See invisible
  You can see invisible monsters, such as ghosts, when they're in view.

Free action
  You can't be paralysed. Deeper down, monsters that paralyse with a touch or
  a spell are among the most dangerous there are.

Feather falling
  You drift down trapdoors and into pits instead of falling, and take no
  damage.

Slow digestion
  You get hungry more slowly.

Trap immunity
  Traps can't hurt you.

Blessed weapon
  A priest or a paladin gets +2 damage with a blessed weapon, as with any
  hafted one. Other classes get nothing from it.

Reflection
  Most bolt spells fired at you bounce away, back towards whoever cast them.
  It fails one time in ten, and balls and breaths can't be reflected.

Pass wall
  You can walk through solid rock.

Ghoul touch
  A monster you hit with your bare hands may fall asleep. It gets a saving
  throw, and monsters that never sleep aren't affected.

Fiery aura
  A monster that hits you in melee takes 2d6 fire damage, unless it's immune
  to fire.

Electric aura
  A monster that hits you in melee takes 2d6 lightning damage, unless it's
  immune to lightning.

Luck
  Raises your saving throw by 10, so you shrug off more of the spells and
  attacks a saving throw protects against.

Drawbacks
---------

Slow healing
  You recover hit points more slowly.

Slow mana recovery
  You recover mana more slowly.

Afraid
  You're always afraid, so you can't fight in melee.

Aggravation
  Wakes nearby monsters and makes them angry.

No teleportation
  You can't teleport. Phase Door, Teleportation and the like won't work on
  you.

Drains experience
  Slowly drains your experience while you wear it.

Can't be removed
  Once it's on, you can't take it off until the curse is broken.

Fragile
  The item may be destroyed if you try to break its curses.

Anti-magic
  You can't cast spells, pray or use mental powers, and most of your
  mutations stop going off.

Cannot eat
  Food does you almost no good: you get a twentieth of the nourishment from
  anything you eat.

Modifiers
---------

Stealth
  How quietly you move. The higher it is, the less often monsters wake up.

Searching
  How readily you notice traps, secret doors and hidden things.

Infravision
  Lets you see warm-blooded monsters in the dark, 10 ft for each point.

Tunneling
  How quickly you dig through rubble, mineral veins and rock.

Speed
  How often you act. At +10 you act twice as often as a normal-speed monster.

Attack speed
  Extra blows with your weapon each round. +1 is one more blow.

Shooting speed
  Extra shots with your launcher, in tenths of a shot. +10 would double how
  often you fire.

Shooting power
  Adds to your launcher's multiplier, so each missile does more damage.

Light radius
  Adds this many squares to how far your light reaches.

Damage reduction
  Takes this much off the damage of every hit you take.

Movement speed
  Lets you move faster than you act. At +1, a step takes half a turn.

Mana
  Raises your maximum mana, by this much for every level at which you can
  cast spells.
