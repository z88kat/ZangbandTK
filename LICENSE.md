# Licence

ZangbandTK is available under the **Angband licence**:

> Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
>
> This software may be copied and distributed for educational, research, and
> not for profit purposes provided that this copyright and statement are
> included in all such copies. Other copyrights may also apply.

In practice that means **non-commercial distribution** — the same terms Zangband
itself carried.

## Why not the GPL as well

Angband is dual-licensed: the GNU General Public License, version 2 *or* the
Angband licence, at your choice. ZangbandTK cannot offer that choice.

Zangband was released under the Angband licence alone, and ZangbandTK
incorporates Zangband material — monsters, artifacts, ego types, and the design
of systems taken from it. The Angband licence is therefore the only option
available here, and it governs the whole of this work.

## Copyright

- **ZangbandTK** — the ZangbandTK contributors, 2026 onwards.
- **Angband** — Ben Harrison, James E. Wilson, Robert A. Koeneke, and everyone
  who has maintained and developed it since. ZangbandTK is built on Angband
  4.2.6.
- **Zangband** — Topi Ylinen, Robert Ruehlmann, and the Zangband DevTeam.
- **AngbandTk and ZAngbandTk** — Tim Baker, for the original Tcl/Tk framework,
  tile engine and interface.

## Exceptions

Parts of a ZangbandTK distribution carry their own terms, which are not the
Angband licence and which the licence above does not override.

### Graphics

**These tilesets have been changed.** None of the four was drawn for this game,
so none had art for the wilderness, and the imported bestiary rendered as ASCII
letters among the pictures. What was done to each — and, importantly, whether it
was a *mapping* (a lookup table pointing at art already in the sheet) or a
*modification* (pixels written into the sheet) — is set out in
[lib/tiles/README](lib/tiles/README). In summary:

| Tileset | Image | What we added |
|---|---|---|
| Gervais 32×32 | **untouched** | a mapping only, onto cells Angband never used |
| Adam Bolt 16×16 | **modified** | one appended row of thirty generated terrain tiles |
| Nomad 16×16 | **modified** | the same |
| Original 8×8 | **modified** | the same |

Nothing already in a sheet is altered: the generated row goes below the existing
art, so every original cell keeps its coordinates and every original pixel its
value. The colours are sampled from the sheet being extended, so the additions
are in that tileset's own palette. All four also gain `.prf` files, which are
mappings and change no art.

- **Adam Bolt's 16×16 tiles** may be redistributed and used for any purpose,
  with or without modification. Modified here, which those terms allow in
  terms.
- **David Gervais' 32×32 tiles** may be redistributed, modified and used only
  under the terms of the
  [Creative Commons Attribution 3.0](http://creativecommons.org/licenses/by/3.0/)
  licence. **Not modified here** — `32x32.png` is byte-identical to Angband's.
  What this game adds is a mapping onto cells the sheet already contained.
- **Shockbolt's 64×64 tiles are not distributed with this game**, and were
  removed in 3.95.0. Angband ships them; we may not. The licence grants use
  "with in-development and released versions of Angband" and withholds it for
  "other games or projects", which is what ZangbandTK is — a separate project,
  separately named, with its own releases and its own savefile format. The
  author offers permission on request and we have not asked, so until somebody
  does, the honest position is not to ship them. See
  <https://angband.readthedocs.io/en/latest/copying.html> for the terms.

  This is not a criticism of the terms. Raymond Gaustadnes drew that tileset for
  Angband and is entitled to say where it goes.
- **The original 8×8 tiles and Nomad's 16×16 tiles** carry no licence statement
  here, and none in Angband's own `copying.rst` either. Recorded as a known gap
  rather than left to be assumed: it is inherited, not settled, and the two sets
  are shipped on the same footing Angband ships them on.

  **Both are modified here**, by the appended row described above. Neither is
  listed among the exceptions below, so both stand on the project licence like
  the rest of the tree — which is a reading of a silence rather than a
  permission granted, and is written down as such. If either author or Angband
  states terms that this reading does not satisfy, the row is one script run
  away from being removed again.

### Sound and fonts

- **The sounds** are licensed under the Creative Commons Attribution 4.0
  licence. They were created by Dubtrain <angband@dubtrain.com> and are
  available in Wave format at <http://www.dubtrain.com/angband/>.
- **The font files** are by Leon Marrick, Sheldon Simms III and Nick McConnell,
  all of whom agreed to their Angband work being released under the GPL.

### Bundled libraries

- **libpng** and **zlib**, shipped as DLLs with the Windows build, are under
  their own permissive licences.
- **The SDL runtime libraries**, if provided with your copy, are under the
  [GNU LGPL](http://www.gnu.org/copyleft/lesser.html). SDL is available from
  <http://www.libsdl.org/>.

## A note for anyone deriving from this

It is considered good practice to retain this statement in derivatives, rather
than — for instance — redistributing Adam Bolt's tiles under the GPL, or making
a variant under only one of the Angband or GPL licences. It keeps changes
shareable between variants.

The manual's [Copying and licence information](https://zangbandtk.com/copying.html)
chapter carries the same terms, and is the version kept up to date with the
game.
