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
   There is a long way to go. Development happens at `GitHub`_.

.. _GitHub: https://github.com/z88kat/ZangbandTK

If you have never played a roguelike before, start with :doc:`a-quick-demo`,
then read :doc:`guide`. If you have played Angband, :doc:`balance` is the
shortest account of what will kill you that would not have before.

Much of :doc:`the manual <manual>` is inherited from Angband and describes
mechanics ZangbandTK keeps unchanged. Where it and :doc:`differences` disagree,
the latter is the authority.


.. toctree::
   :maxdepth: 1
   :hidden:

   start
   manual
   differences
   meta
   hacking/index


Where to go
===========

.. grid:: 1 2 2 2
   :gutter: 3

   .. grid-item-card:: Getting Started
      :link: start
      :link-type: doc

      New to roguelikes, or new to this one. A walkthrough of a character's
      first few minutes, then the players' guide.

   .. grid-item-card:: What's Different
      :link: differences
      :link-type: doc

      What ZangbandTK does that Angband does not: the wilderness, the
      bestiary, the lethality.

   .. grid-item-card:: The Manual
      :link: manual
      :link-type: doc

      The reference: character creation, the dungeon, combat, commands and
      options.

   .. grid-item-card:: For Developers
      :link: hacking/index
      :link-type: doc

      Building from source, the data file layout, and debugging.
