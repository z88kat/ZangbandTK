> **Note.** This is the original project statement, kept as written. Where it differs from
> [decisions.md](decisions.md), the decision log is authoritative. Two points have since
> changed materially: the **clean-room approach below was dropped** (DEC-20 — Zangband's
> source may be read and ported), and the archive descriptions in Phase 3 are corrected in
> the decision log's archive note.

## Goal

I used to play zangband when i was a student. Sadly development stopped back in 2004.
I would like to re-build zangbandtk (tk/tcl version) based on the current sources of angband.

One of my goals is to bring ZangbandTK back to Roger Zelazny’s Amber novels, the original concept but somehow got diverted from this goal mixing Lovecraft, Tolken, and probably bunch of other RPG's. I would to turn this back toward the Amber novels.

https://en.wikipedia.org/wiki/The_Chronicles_of_Amber

## What to do

Make a development plan. I would take a clean room approach to deveopment and create a development plan. Using that plan against the current code we re-build the "spirt" of zangband. So this not a direct port or code comparison against the current release. It is a determination of the differences, creating a development requirements, writing a development plan and then running that against the current angband source (which is here in this project). Once we have that I would like to apply tk/tcl to finally build ZangbandTK, using the latest release of tk/tcl

An outstanding question is how destructive to the original angband source we need to make. Can we continue to pull changes from the original angband repository or are the modification too extenstive?

## Phase 1

The last available source code for zandband (unreleased) is in /archive/zangband. This is version 2.7.5-preview.

To find the differences we need to check against the original sources that zangband is based upon which is Angband 2.8.1 (03/04/97) and I have put those files in /archive/angband-281

The task would be to try and identify what are the gaming mechanics which are different between both version and put those into a requirements plan.

You should ignore the other files in this project which contain the current angband release. Will we use those later.

## Phase 2

From the requirements, create a development plan against the current source code of angband, which we have in this project. I have folked the current release of angband and this is what you have here.

## Phase 3

The end goal is to create a tk/tcl version to zangband, called ZangbandTK.

I have managed to find the original source code of the ZandbandTK version 2.9.2. You will find this in /archive/tk/AngbandTk

The images (I believe) are in /archive/tk/image

The sound files (wav) I believe we can re-use those too are in /archive/tk/sounds1, /archive/tk/sounds2, /archive/tk/sounds3, /archive/tk/sounds4

There is a bunch of other stuff in /archive/tk/ but i am not sure if any of that is useful

The original release of ZandbandTK can be found in /archive/zandbandtk

Information and screenshots of the original release can be found at https://www.old-games.com/download/4122/zangbandtk

We should make an analysis of these files in order to create a tk/tcl version of the project. We need to build a development plan for that. We are going to use tk/tcl 9.0.4

The sources for tcl are in tcl9.0.4 and tk is in tk9.0.4 we need to check if these are Mac ARM compatible in order to compile them.

## Orginal Documentation

https://web.archive.org/web/20220527225941/http://zangband.org/
https://web.archive.org/web/20220430102307/http://zangband.org/docs/general.txt
https://web.archive.org/web/20220420164303/http://www.zangband.org/docs/birth.txt
https://web.archive.org/web/20220420164304/http://www.zangband.org/docs/charattr.txt
https://web.archive.org/web/20220420164251/http://www.zangband.org/docs/town.txt
https://web.archive.org/web/20220420164309/http://www.zangband.org/docs/dungeon.txt
https://web.archive.org/web/20220430102308/http://zangband.org/docs/objects.txt
https://web.archive.org/web/20220420164250/http://www.zangband.org/docs/monster.txt
https://web.archive.org/web/20220420164251/http://www.zangband.org/docs/attack.txt
https://web.archive.org/web/20220420164251/http://www.zangband.org/docs/defend.txt
https://web.archive.org/web/20220420164302/http://www.zangband.org/docs/magic.txt
https://web.archive.org/web/20220430102306/http://zangband.org/docs/command.txt
https://web.archive.org/web/20220420164252/http://www.zangband.org/docs/commdesc.txt
https://web.archive.org/web/20220420164302/http://www.zangband.org/docs/option.txt
https://web.archive.org/web/20220420164257/http://www.zangband.org/docs/pref.txt
https://web.archive.org/web/20220420164252/http://www.zangband.org/docs/wizard.txt
https://web.archive.org/web/20220420164300/http://www.zangband.org/docs/version.txt

https://web.archive.org/web/20220508202606/http://zangband.org/spoilers/racetab.txt
https://web.archive.org/web/20220508202607/http://zangband.org/spoilers/raceclas.txt
https://web.archive.org/web/20220420164301/http://www.zangband.org/spoilers/racepow.txt
https://web.archive.org/web/20220420164302/http://www.zangband.org/spoilers/classtab.txt
https://web.archive.org/web/20220420164256/http://www.zangband.org/spoilers/clasrace.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/stats.txt

https://web.archive.org/web/20220508202606/http://zangband.org/spoilers/life.txt
https://web.archive.org/web/20220622234645/http://zangband.org/spoilers/arcane.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/sorcery.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/nature.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/trump.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/chaos.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/death.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/mind.txt

https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/randabil.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/chaospat.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/mutation.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/tycurse.txt
https://web.archive.org/web/20220527225941/http://zangband.org/spoilers/nightmare.txt

## Websites

https://en.wikipedia.org/wiki/The_Chronicles_of_Amber

## Game Info

https://goldenageofgames.com/zangbandtk/

## Playable

https://dos.zone/zangband-dec-15-1994/

## PC Adventure Game

https://www.myabandonware.com/game/nine-princes-in-amber-c0
https://gamefaqs.gamespot.com/pc/567479-nine-princes-in-amber/faqs/15640

Yer, you going to need that walkthough.

## Books

https://www.amazon.co.uk/Chronicles-Amber-S-F-MASTERWORKS/dp/1473222168/
https://www.amazon.co.uk/Second-Chronicles-Amber-S-F-MASTERWORKS/dp/147322215X/

## Release

To cut the release

git push origin master
git tag 3.24.1
git push origin 3.24.1

## Pre-Release

git push origin master
git tag -a 3.5.1-rc1 -m "Windows build test"
git push origin 3.5.1-rc1

## Amber m´Material

https://adrp.proboards.com

The Betancourt Prequels:

1. The Dawn of Amber (2002)
2. Chaos and Amber (2003)
3. To Rule in Amber (2004)
4. Shadows of Amber (2005)
5. Sword of Chaos (never written)

The Zelazny novels:
THE CORWIN BOOKS:

1. Nine Princes in Amber (1970)
2. The Guns of Avalon (1972)
3. Sign of the Unicorn (1975)
4. The Hand of Oberon (1976)
5. The Courts of Chaos (1978)
   THE MERLIN BOOKS:
6. The Trumps of Doom (1985)
7. Sign of Chaos (1986)
8. Blood of Amber (1987)
9. Knight of Shadows (1989)
10. Prince of Chaos (1991)

The Zelazny Short Stories:

1. Prolog to "The Trumps of Doom" (Merlin POV; 1985)
2. The Salesman's Tale (Luke POV; 1994)
3. Coming to a Cord (Frakir POV; 1995)
4. The Shroudling and the Guisel (Merlin POV; 1995)
5. Blue Horse, Dancing Mountains (Corwin POV; 1995)
6. Hall of Mirrors (Corwin POVl; 1996)

Other RZ Amber Writings of Interest:

1. Introduction to the Graphic Novel "Nine Princes in Amber"
2. Introduction to "Seven No-Trump"
3. Introduction to "The Black Road War"

Amber Diceless Role Playing:
ADRP Core Rulebook (1991)
++ ADPR Core Rulebook -- PDF

Shadow Knight (1993)
++ Shadow Knight -- PDF

Rebma Sourcebook (never printed)

Amberzine #1 (March 1992)
Amberzine #2 (August 1992)
Amberzine #3 (April 1993)
Amberzine #4 (August 1993)
Amberzine #5 (November 1993)
Amberzine #6 (February 1994)
Amberzine #7 (July 1994)
Amberzine #8 (November 1995)
Amberzine #9 (January 1996)
Amberzine #10 (October 1997)
Amberzine #11 (May 2001)
Amberzine #12 (March 2005)

Comic Books:

1. Nine Princes in Amber #1/3 (1996)
2. Nine Princes in Amber #2/3
3. Nine Princes in Amber #3/3
4. The Guns of Avalon #1/3
5. The Guns of Avalon #2/3
6. The Guns of Avalon #3/3

Magazines:
Review of ADRP (Pyramid #2)
Review of Shadow Knight (Pyramid #6)

"An Interview with designer Erick Wujcik" (Dangerous Journeys #1 / 1992)
"A short history of Amber Diceless Role Playing" (Dangerous Journeys #2 / 1992)

"Child's Play I" (Shadus #33 / 1996?)
"Child's Play II" (Shadus #34 / 1996?)

"The InQuest world guide to Amber" (InQuest / May 1997)

Miscellaneous:
The Illustrated Roger Zelazny (1978)

Le Tarot d'Ambre (Tarot Deck / 1994)
English Translation book (never published)

Roger Zelazny's Visual Guide to Castle Amber (Randall / 1988)
"Seven No-Trump" Crossroads Adventure (Randall / 1988)
"The Black Road War" Combat Command (Randall / 1988)

The Complete Amber Sourcebook (Krulik / 1996)

Amber Minister of the Interior Geologic Survey Map (date unknown)

## Follow up Ideas

Sometimes walking over the wilderness feels like a slog, trying to discover the world. Its dangeous too.

Knowing the map helps, a lot. Maybe we can add map "fragments" which can be found to reveal a part of the overworld map, or you can just buy those "fragments" in a shop (for a high price). This then reveals part of the map.

## NDS Build

We are limited by RAM on the NDS. Here is my idea.

We abandon the wilderness build. The doors are locked by "magic" and cannot be opened until the "elipse of the moon".

We make all towns and dungeons available via the mage towsers. All towns and dungeons are available for travel at the start of the game, you still have to pay the travel costs, so you cannot travel immediately without a bit of work to gain gold. We can make cities more expensive then towns, deeper dungeons more expensive then shorter ones.

This means we still keep the flavour of zangband, but without the wilderness. It's a trade-off.

This "might" allow us to make a stable NSD build. It is something to investigate.

But, we might still be able to support wilderness with additional hardware. I have a RAM pack we could consider to make a special build which includes the wilderness for the "ram pack" edition. We need to evaluate if the wilderness fits into memory with the ram pack?

Note: There is a bug, the font on the NDS looks like crap, its really blury and hard to read. At first i just figured it was the emulator, but on the hardware its the same, so whatever font we using, it does not work. I checked the original angband NDS release, it has the same problem, so it is not something we have introduced. It looks broken at source. It also appears we are not using the full width of the screen (again broken in angband).

## 3DS Build

We follow the same principles that work with the NDS.

## Spectrum Next Build

https://en.wikipedia.org/wiki/ZX_Spectrum_Next
https://wiki.specnext.dev/Specifications
https://www.specnext.com/category/resources/resources_coding/

Is this even viable to build a version of ZangbandTK for the spectrum next.

The spectrum next has more proceessing power and memory then the original spectrum.

## Commmadore 64

Maybe a step too far. There was actually a build of angband for the commadore 64 the original sources are here:

./archive/AngbandPlus64
