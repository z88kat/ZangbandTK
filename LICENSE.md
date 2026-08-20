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

- **Adam Bolt's 16×16 tiles** may be redistributed and used for any purpose,
  with or without modification.
- **David Gervais' 32×32 tiles** may be redistributed, modified and used only
  under the terms of the
  [Creative Commons Attribution 3.0](http://creativecommons.org/licenses/by/3.0/)
  licence.
- **Shockbolt's 64×64 tiles** are copyright © Raymond Gaustadnes 2012, in
  `lib/tiles/shockbolt/64x64.png`. Permission is granted to use the tileset with
  in-development and released versions of Angband; to distribute and copy it
  with them, as long as no fee is charged; and to incorporate tiles the author
  designed for variants of Angband and distribute them under those terms.

  Permission is **not** granted to modify the tileset without the author's
  permission, to incorporate tiles designed for ToME that do not appear in the
  Angband tileset, or to use or distribute the tileset with other games or
  projects without explicit permission from the author. Non-commercial projects
  may be granted permission on request; commercial use requires a non-exclusive
  licence from the author.

  Some tiles in that sheet were resized from tiles David Gervais made for the
  32×32 set.

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
