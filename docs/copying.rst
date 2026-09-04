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

* The SDL runtime libraries (if provided with your copy of the game) are under
  the following licence:

    The Simple DirectMedia Layer (SDL for short) is a cross-platform library designed to make it easy to write multi-media software, such as games and emulators.

    The Simple DirectMedia Layer library source code is available from: http://www.libsdl.org/

    This library is distributed under the terms of the GNU LGPL license: http://www.gnu.org/copyleft/lesser.html

.. note::

   **The tilesets shipped with this game have been changed.** None of them was
   drawn for ZangbandTK, so none had art for the wilderness, and the imported
   bestiary rendered as ASCII letters among the pictures.

   Two kinds of change, and the difference decides whether a licence forbidding
   modification has been respected. A **mapping** adds a ``.prf`` file pointing
   at art already in the sheet and alters no image. A **modification** writes
   pixels into it.

   * **Gervais' 32x32 is mapped only** — the image is byte-identical to
     Angband's. Thirteen terrain features point at cells Angband never used.
   * **Adam Bolt's, Nomad's and the original 8x8 are modified**: one row of
     thirty generated terrain tiles is appended to the bottom of each. Nothing
     already in those sheets is touched — every original cell keeps its
     coordinates and every original pixel its value — and the colours are
     sampled from the sheet being extended, so the additions are in that
     tileset's own palette.
   * **All four gain ``.prf`` files**, which are mappings and change no art.

   ``lib/tiles/README`` records this per tileset, and
   ``scripts/tiles/make-terrain.py`` is the script that does it.

* Adam Bolt's (16x16) graphics may be redistributed and used for any purpose, with or without modification.  **Modified here**, which those terms allow in terms.

* David Gervais' (32x32) graphics may be redistributed, modified, and used only under the terms of the `Creative Commons Attribution 3.0 <http://creativecommons.org/licenses/by/3.0/>`_ licence.  **Not modified here**: what this game adds is a mapping onto cells the sheet already held.

* **Shockbolt's (64x64) graphics are not distributed with this game.** Angband ships them; ZangbandTK may not. The licence grants use "with in-development and released versions of Angband" and expressly withholds it for "other games or projects" — and this is another project: separately named, separately released, with a savefile format of its own. The author offers permission on request and we have not asked, so the tileset was removed in 3.95.0 rather than shipped on an assumption. The terms are at `Angband's copying page <https://angband.readthedocs.io/en/latest/copying.html>`_.

  Nothing about this is a complaint. Raymond Gaustadnes drew that set for Angband and is entitled to say where it goes.

* **The original (8x8) and Nomad's tiles** carry no licence statement here, and none in Angband's own copying page. That is a known gap, inherited rather than settled, and is written down instead of assumed.  **Both are modified here** by the appended row described above, on the reading that neither is carved out as an exception and both therefore stand on the project licence like the rest of the tree.  That is a reading of a silence rather than a permission granted; if terms appear that it does not satisfy, the row is one script run away from being taken out again.

* The sounds are licenced under the Creative Commons Attribution 4.0 licence.  They were created by Dubtrain <angband@dubtrain.com>. You can find them in Wave format at http://www.dubtrain.com/angband/.

* The font files are all by Leon Marrick and/or Sheldon Simms III and/or Nick McConnell, all of whom have agreed to their Angband work being released under the GPL.

A note for anyone deriving from this
====================================

It is considered good practice to retain this statement in derivatives, rather
than — for instance — redistributing Adam Bolt's tiles under the GPL, or making
a variant under only one of the Angband or GPL licences. It keeps changes
shareable between variants.

This chapter and the ``LICENSE.md`` in the source carry the same terms. This one
is the version kept up to date with the game.
