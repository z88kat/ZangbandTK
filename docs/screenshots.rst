===========
Screenshots
===========

These are captured from the running game, not drawn. ZangbandTK renders as
characters with colour attributes, so what you see below *is* the screen — every
glyph in the position and colour the game put it in, at any size you care to
zoom to.

The world
=========

The whole of one world, seen from the overhead map. A game generates this from a
single seed and it comes back the same every time (:doc:`wilderness`).

.. image:: screenshots/world-map.svg
   :alt: The overhead map of a whole ZangbandTK world, 129 blocks square, showing
         sea, coast, forest, mountain and open country, with towns and dungeon
         mouths marked.
   :width: 100%

Blue is sea and light blue the shallows you can wade. Green is grassland, light
green forest, umber the mountains that are impassable. The ``@`` is the character
and ``>`` a dungeon mouth. The towns show as blocks rather than points, because
they are: a great city is 132 grids across, which is most of nine blocks.

This is the map with everything known. In play you see only what you have been
near — the map fills in behind you as you walk, and that is most of what the
:doc:`magetower <towns>` is for.

The surface
===========

Not a town level with a wilderness somewhere else. The same map, scrolling as you
walk, with the town standing in it.

.. image:: screenshots/the-surface.svg
   :alt: The wilderness surface with a walled town in it, its numbered shops
         visible through the gate, roads leading away, and open country around.
   :width: 100%

The numbers are shops and the ``+`` symbols are services — an inn, a healer, a
magesmith. The road leaves by the gate and goes somewhere: every town and every
dungeon mouth in the world is on the network, which is how you find the next one.

Walk far enough in any direction and the window scrolls; walk far enough west and
you run out of land, then out of world.

How these were made
===================

With the game's own renderer, driven headlessly. The test front end writes every
character, position and colour the game draws; a small script replays that into a
grid and emits SVG using Angband's own palette. Two of the debug commands set the
scene — *Know every place* to fill the map in, and a larger terminal than a player
would normally use so the whole world fits in one frame.

That means these cannot drift from the game. They are not screenshots of a build
somebody had lying about; regenerating them runs the current code.

Still to come
=============

**The coast and deep water**, which is worth a picture of its own. **The
bestiary**, showing the monsters Angband does not have. **A vampiric or chaotic
weapon** discharging, since those mechanics have no Angband equivalent.
