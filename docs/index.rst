:html_theme.sidebar_secondary.remove: true

=======================================
Rebuilding Zangband on a modern Angband
=======================================

Zangband was one of the great Angband variants — a wilderness to cross, towns to
visit, mutations, pets, chaos patrons, and a bestiary drawn from Roger Zelazny's
Amber and H. P. Lovecraft's Mythos as much as from Tolkien. Its development
stopped in 2005, at version 2.7.5-pre1.

Angband did not stop. It is now at 4.2.6, with twenty years of better level
generation, a real data-driven architecture, and a proper object property
system that Zangband never had.

ZangbandTK puts the first on top of the second. It is not a port: Zangband's
2005 codebase is not what is worth preserving. Its *character* is.

.. note::

   **Status: early.** The game is playable, already feels different from
   Angband, and has the wilderness — Zangband's defining feature — under it.
   There is a long way to go, and :doc:`features` says exactly how far.
   Development happens at `GitHub`_.

.. _GitHub: https://github.com/z88kat/ZangbandTK


.. toctree::
   :maxdepth: 1
   :hidden:

   features
   screenshots
   download
   releases
   documentation


Start here
==========

.. grid:: 1 2 2 2
   :gutter: 3

   .. grid-item-card:: Features
      :link: features
      :link-type: doc

      What is in the game now — the wilderness, the bestiary, the lethality —
      and what is still to come.

   .. grid-item-card:: Download
      :link: download
      :link-type: doc

      No binaries yet. Two commands to build it from source on macOS, and what
      to know before you start.

   .. grid-item-card:: Documentation
      :link: documentation
      :link-type: doc

      The manual, in four parts. Start with the demonstration if roguelikes are
      new to you.

   .. grid-item-card:: Release log
      :link: releases
      :link-type: doc

      The development log, by milestone. M0 to M4 are complete.


What makes it Zangband
======================

Three things, in the order you will notice them.

**Monsters die sooner, and so do you.** Every monster carries 73% of Angband's
hit points and 50% of its armour class — the measured difference between Zangband
2.7.5 and the Angband it forked from. Fights resolve in fewer turns, in whichever
direction they were going. :doc:`balance` is the full account.

**There is a world, not a staircase.** A wilderness 2064 grids square, generated
from a seed and never stored, with the town standing in it and roads out of it.
Deep water can be waded and drowned in, the world ends in open sea, and what you
drop in the country stays where you left it until somebody finds it.
:doc:`wilderness` covers it.

**The bestiary is not Tolkien's alone.** 389 monsters imported from Zangband,
including the princes of Amber and the Mythos deities, alongside 51 artifacts and
18 ego types. :doc:`monsters` and :doc:`objects` have the detail.
