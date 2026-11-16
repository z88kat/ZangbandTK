===================
Play in the browser
===================

The game runs in a browser, with nothing to install and nothing to download by
hand: `zangbandtk.com/play <https://zangbandtk.com/play/>`_.

It's the same game, the same C code, compiled to WebAssembly instead of to a
machine's own instructions. Nothing is emulated and nothing runs on a server.
The browser runs the compiled code directly, the same way it would run any
other program it was handed, and the wilderness is generated in the tab you
have open.

The page
========

The map is in the middle, in tiles. Your character is on the left: health and
mana as bars, then your stats, level, armour, speed, gold and where you are.
Your pack, equipment and quiver are on the right, each item with its tile and
its weight. Messages scroll along the bottom, and the ones from your last move
are brighter than the older ones. The line above the map is the game's prompt
line, and the line under it is the status bar.

When the game asks you something, it asks in a box over the map. Choosing an
item gives you a list to click, or you can type the item's letter as usual.
There are tabs for your equipment, quiver and the floor when the question
allows them, and ``/``, ``|`` and ``-`` move between them as they do in the
other builds. Yes-or-no questions, numbers and text, and the menus of item
actions and powers, come up in boxes too. Escape cancels any of them.

Click a square on the map and you walk to it.

Some screens are still the game's own text screens: the shops, the character
sheet, the options, the knowledge menus and the spell lists. While one is open
it takes the map's place, drawn the way a terminal would draw it, and the map
comes back when you leave. They're being turned into proper pages one at a
time.

Making a character
==================

Character creation has a page of its own. Race, class and realm are lists,
with the game's description of whichever one you're on and, for a realm, the
spells your class would learn from it, book by book. A portrait shows the
character as it would look: the same pictures as the Tk front end's, drawn as a
Ranger until you've picked a class, because a Ranger has nothing on its head to
hide the hair, ears and horns that tell the races apart. After those come the
stats, bought with points or rolled, then a name, a history you can rewrite,
and a summary to check before you begin.

*Back* steps back a stage, *Random* picks a race or class for you and *Finish
at random* picks everything that's left. The text screen's keys work as well:
the arrows and Return, Escape, ``*``, ``@`` and ``=``. The birth options are
still the game's text screen. When a character dies, *New Game* on the death
screen lets you play the same character again, or change its name and history
first.

Tiles
=====

The *Tiles* menu at the top right changes the tileset straight away, and the
browser remembers your choice. Five of the six tilesets are there: the original
8×8 tiles, Adam Bolt's, David Gervais', Neon with its colour-blind version, and
the 1982 set. You start with Neon. Nomad's set is left out because its tiles
are twice as tall as they are wide, and the page draws every tile square.

What it costs to start
======================

About 7 MB on the first visit, and it's cached after that. Roughly 2.5 MB is
compiled code, 4.2 MB is game data (the monsters, objects, dungeon profiles,
the help files and five tilesets) and the rest is the page and its font. The
page shows how far the download has got while it loads.

Savefiles
=========

Your game is saved in the browser, and it survives closing the tab. A
character from the earlier version of this page opens in this one, because
both keep the savefile in the same place.

It isn't a file you can find, though. The savefile lives in the browser's own
storage, so it belongs to one browser on one machine. A character rolled in
Chrome isn't there in Firefox, or on your phone, and clearing site data for the
domain deletes it along with everything else the site has stored. Nothing is
uploaded anywhere, so if it goes, it can't be recovered.

If you care about a character, play a native build from :doc:`download`, where
the savefile is a file on your disk that you can copy.

What is missing
===============

The browser version is smaller than the native ones.

There's no sound. It's compiled out of the build entirely, which also keeps
3 MB of samples out of the download.

There are no extra windows. The native builds can show the monster list, the
object list and the rest in windows of their own, and the page has no place
for them yet.

There's no fullscreen. The page already fills the window, and the browser
reserves Escape for leaving fullscreen. This game needs that key far too often
to lend it out.

The page is laid out when it loads. Make the window smaller and the map shrinks
to fit; make it bigger and the map stays the size it was. Either way you see the
same part of the level until the page next loads.

Everything else is as it is in the native builds: the same wilderness,
bestiary, lethality and savefile format.

Which browsers
==============

Any current Chrome, Edge, Firefox or Safari. WebAssembly has been in all of them
since 2017, and this build asks for nothing newer.

You'll need a keyboard, though. The game is driven by single keystrokes. The
mouse walks you about and answers the boxes, but there are no on-screen
controls for the rest, so a phone or a tablet will load it and then leave you
with no way to play.

Which version
=============

The browser version follows the latest source, not the latest release: it's
rebuilt and republished whenever the game changes. That makes it
the newest ZangbandTK there is, and the least settled. The tagged builds on
:doc:`download` are the ones the :doc:`release log <releases>` describes.
