:html_theme.sidebar_secondary.remove: true

=======================================
Rebuilding Zangband on a modern Angband
=======================================

Zangband was one of the great Angband variants. It had a wilderness to cross,
towns to visit, mutations, pets, chaos patrons, and a bestiary drawn as much
from Roger Zelazny's Amber and H. P. Lovecraft's Mythos as from Tolkien. Its
development stopped in 2005, at version 2.7.5-pre1.

Angband kept going. It's now at 4.2.6, with twenty years of better level
generation, a data-driven architecture, and a proper object property system
that Zangband never had.

ZangbandTK puts the first on top of the second. It isn't a port, because
Zangband's 2005 code wasn't worth keeping. What it rebuilds is the game's
*character*.

.. note::

   **Status: feature-complete.** Everything Zangband is known for is in: the
   wilderness, the towns and their services, thirteen dungeons, the bestiary,
   mutations, chaos patrons, seven realms of magic, pets and nightmare mode.
   What's left is polish, and a second Mac front end modelled on the original
   ZangbandTk. :doc:`features` lists what's in and what isn't, and every
   release so far is still marked as a pre-release. Development happens on
   `GitHub`_.

.. _GitHub: https://github.com/z88kat/ZangbandTK


.. toctree::
   :maxdepth: 1
   :hidden:

   features
   screenshots
   play
   download
   releases
   diary
   documentation


Start here
==========

.. grid:: 1 2 2 2
   :gutter: 3

   .. grid-item-card:: Features
      :link: features
      :link-type: doc

      Everything that's in the game now, and what's still to come.

   .. grid-item-card:: Play in the browser
      :link: play
      :link-type: doc

      The same game, compiled to WebAssembly. There's nothing to install, and
      it saves where you left off.

   .. grid-item-card:: Download
      :link: download
      :link-type: doc

      A disk image for macOS, zips for Windows and an AppImage for Linux. Or
      build it from source with two commands.

   .. grid-item-card:: Documentation
      :link: documentation
      :link-type: doc

      The manual, in four parts. If roguelikes are new to you, start with the
      demonstration.

   .. grid-item-card:: Release log
      :link: releases
      :link-type: doc

      What changed in each version, by milestone. M0 to M11 are complete,
      and the Tk front end is at its last milestone.


What makes it Zangband
======================

The first thing you'll notice is that monsters die sooner, and so do you.
Every monster has 73% of Angband's hit points and 50% of its armour class,
which is the difference measured between Zangband 2.7.5 and the Angband it
forked from. Fights are over in fewer turns, whichever way they were going.
:doc:`balance` has the full story.

Then there's the world. Angband is a town and a staircase; here there's a
wilderness 2064 grids square, generated from a seed and never stored, with the
town standing in it and roads leading out. Deep water can be waded into and
drowned in, the world ends in open sea, and what you drop in the country stays
where you left it until somebody finds it. :doc:`wilderness` covers it.

And the monsters aren't all Tolkien's. There are 387 imported from Zangband,
including the princes of Amber and the Mythos deities, along with 51 artifacts
and 16 ego types. :doc:`monsters` and :doc:`objects` have the details.
