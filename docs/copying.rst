===============================
Copying and licence information
===============================

ZangbandTK is available under the **Angband licence**:

  Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke

  This software may be copied and distributed for educational, research, and not for profit purposes provided that this copyright and statement are included in all such copies.  Other copyrights may also apply.

In practice that means **non-commercial distribution** — the same terms Zangband
itself carried.

.. important::

   **ZangbandTK is not dual-licensed, where Angband is.** Angband offers a
   choice between the `GNU General Public License, version 2
   <http://www.gnu.org/licenses/gpl-2.0.html>`_ and the Angband licence.
   ZangbandTK cannot offer that choice: Zangband was released under the Angband
   licence alone, and this game incorporates Zangband material — monsters,
   artifacts, ego types, and the design of systems taken from it. The Angband
   licence is therefore the only option available here, and it governs the whole
   of this work.

Copyright
=========

- **ZangbandTK** — the ZangbandTK contributors, 2026 onwards.
- **Angband** — Ben Harrison, James E. Wilson, Robert A. Koeneke, and everyone
  who has maintained and developed it since. ZangbandTK is built on Angband
  4.2.6.
- **Zangband** — Topi Ylinen, Robert Ruehlmann, and the Zangband DevTeam.
- **AngbandTk and ZAngbandTk** — Tim Baker, for the original Tcl/Tk framework,
  tile engine and interface.

Exceptions
==========

Parts of a ZangbandTK distribution carry their own terms, which are not the
Angband licence and which the licence above does not override:

* The libraries the downloads carry are under their own licences, and every
  download that contains other people's software has a
  ``THIRD-PARTY-LICENSES.txt`` beside ``LICENSE.md`` with each of them in full
  (``LICENSES.TXT`` in the DOS zip). ZangbandTK carries Tcl and Tk, and on
  Windows and Linux SDL2 with the libraries it plays sound and music through.
  The Windows and Linux builds of Zangband carry SDL2, libpng, zlib or
  ncurses, and the DOS, DS and 3DS builds have their C libraries linked in.
  The same texts are in the ``licenses`` folder of the source.

.. note::

   **The tilesets shipped with this game have been changed.** None of them was
   drawn for ZangbandTK, so none had art for the wilderness, and the imported
   bestiary rendered as ASCII letters among the pictures.

   Two kinds of change, and the difference decides whether a licence forbidding
   modification has been respected. A **mapping** adds a ``.prf`` file pointing
   at art already in the sheet and alters no image. A **modification** writes
   pixels into it.

   * **Gervais' 32x32 has one row added**: four service buildings it had no
     art for, and the Statue and Figurine Stores' doors, all drawn by a
     script. Everything above that row is Angband's sheet, pixel for pixel,
     and thirteen terrain features point at cells Angband never used.
   * **Adam Bolt's, Nomad's and the original 8x8 are modified**: two rows of
     generated tiles, terrain and service buildings, are appended to the
     bottom of each. Adam Bolt's also gains three rows of the tiles Bolt drew
     for Zangband (sixty-five uniques, the magical figurine and ten statues),
     and the 8x8 gains one row of Zangband's own figurine and statues.
     Nothing already in those sheets is touched: every original cell keeps
     its coordinates and every original pixel its value. The generated tiles
     take their colours from the sheet being extended, so they are in that
     tileset's own palette.
   * **All four gain ``.prf`` files**, which are mappings and change no art.

   ``lib/tiles/README`` records this per tileset, and
   ``scripts/tiles/make-terrain.py``, ``make-services.py`` and
   ``import-zangband-tiles.py`` are the scripts that do it.

* Adam Bolt's (16x16) graphics may be redistributed and used for any purpose, with or without modification.  **Modified here**, which those terms allow.

* David Gervais' (32x32) graphics may be redistributed, modified, and used only under the terms of the `Creative Commons Attribution 3.0 <http://creativecommons.org/licenses/by/3.0/>`_ licence.  **Modified here**, which those terms allow: one row of generated buildings is appended below Angband's sheet, which is otherwise unchanged, as described above.

* **David Gervais' 2001 tiles** (32x32), the sheets Gervais drew for AngbandTk and ZAngbandTk, are under the same `Creative Commons Attribution 3.0 <http://creativecommons.org/licenses/by/3.0/>`_ licence: Gervais' own tile library publishes them, byte for byte the copies this project took from the 2001 archive. **Modified here**: ``scripts/tiles/build-gervais2001.py`` cuts them into cells, dims each for the game's lighting and lays them out below Angband's Gervais sheet as a tile set of their own, and assembles roofs and roads from the 2001 pieces. ``lib/tiles/README`` lists every change. Three more sheets from the same archive carry nobody's name (a town building, mineral veins and the Pattern). They stand on the project licence by the same reading of a silence as the original and Nomad's tiles below, and are kept as source; nothing in the game draws them.

* **Shockbolt's (64x64) graphics are not distributed with this game.** Angband ships them; ZangbandTK may not. The licence grants use "with in-development and released versions of Angband" and expressly withholds it for "other games or projects" — and this is another project: separately named, separately released, with a savefile format of its own. The author offers permission on request and we have not asked, so the tileset was removed in 3.95.0 rather than shipped on an assumption. The terms are at `Angband's copying page <https://angband.readthedocs.io/en/latest/copying.html>`_.

  Nothing about this is a complaint. Raymond Gaustadnes drew that set for Angband and is entitled to say where it goes.

* **The original (8x8) and Nomad's tiles** carry no licence statement here, and none in Angband's own copying page. That is a known gap, inherited rather than settled, and is written down instead of assumed.  **Both are modified here** by the appended row described above, on the reading that neither is carved out as an exception and both therefore stand on the project licence like the rest of the tree.  That is a reading of a silence rather than a permission granted; if terms appear that it does not satisfy, the row is one script run away from being taken out again.

* **The 1982 tiles (32x32) were made for this game.** An AI image generator drew them to a brief written for ZangbandTK, and ``scripts/tiles/build-1982.py`` cut them down, recoloured them and finished them. Nothing in them was taken from another tile set, so they stand on the project licence like the rest of the tree.

* **The Neon tiles (32x32) were made for this game as well.** The same kind of generator drew them as neon signs to a brief of their own, and ``scripts/tiles/build-neon.py`` cut them down, coloured them and finished them. They take nothing from another tile set either, so the project licence covers them too. They replaced Neon's first tiles, which were typed in by hand for this game, in November 2026.

* **The character portraits (64x64) were made for this game too.** An AI image generator drew them to a brief written for ZangbandTK, and ``scripts/portraits/build-portraits.py`` cut them down, snapped them to a palette and made the sheets the game reads. They take nothing from anyone else's art, so the project licence covers them. They replaced the first portraits, which that script put together from text grids written for this game, in November 2026.

* **The isometric view's art is this project's own.** ``scripts/tiles/make-iso.py`` draws all of it: the terrain in its three styles, and the letters on its counters, which are taken from the DS port's font in ``src/nds``. So it stands on the project licence like the rest of the tree. ZAngbandTk 2.4 had an isometric view as well, drawn from an isometric sheet of David Gervais', and nothing of it is used here. This page used to say that sheet came with no more than a permission given to that project. It doesn't: Gervais' own tile library publishes it under Creative Commons Attribution 3.0, like the rest of Gervais' tiles. What stands on the view (monsters, items, the character) is the chosen tile set's own tiles, under that set's terms above.

* The sounds are licenced under the Creative Commons Attribution 4.0 licence.  They were created by Dubtrain <angband@dubtrain.com>. You can find them in Wave format at http://www.dubtrain.com/angband/.

* The font files are all by Leon Marrick and/or Sheldon Simms III and/or Nick McConnell, all of whom have agreed to their Angband work being released under the GPL.

* **ZangbandTK's own fonts**, in ``lib/tcl/fonts``, are other people's work, unmodified, under the `SIL Open Font License 1.1 <https://openfontlicense.org/>`_: JetBrains Mono (the JetBrains Mono Project Authors), IBM Plex Mono (IBM), Source Code Pro (Adobe), Space Mono (the Space Mono Project Authors) and Red Hat Mono (the Red Hat Project Authors). Each font's licence and copyright are beside it in that folder and in ``THIRD-PARTY-LICENSES.txt``. The licence lets them be bundled with software and passed on with it, but not sold on their own.

A note for anyone deriving from this
====================================

It is considered good practice to retain this statement in derivatives, rather
than — for instance — redistributing Adam Bolt's tiles under the GPL, or making
a variant under only one of the Angband or GPL licences. It keeps changes
shareable between variants.

This chapter and the ``LICENSE.md`` in the source carry the same terms. This one
is the version kept up to date with the game.
