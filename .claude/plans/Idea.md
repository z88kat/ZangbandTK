> **Note.** This is the original project statement, kept as written. Where it differs from
> [decisions.md](decisions.md), the decision log is authoritative. Two points have since
> changed materially: the **clean-room approach below was dropped** (DEC-20 — Zangband's
> source may be read and ported), and the archive descriptions in Phase 3 are corrected in
> the decision log's archive note.

## Goal

I used to play zangband when i was a student. Sadly development stopped back in 2004.
I would like to re-build zangbandtk (tk/tcl version) based on the current sources of angband.

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

The original release of ZandbandTK can be found in /arhive/zandbandtk

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
