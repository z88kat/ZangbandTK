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

What it costs to start
======================

About 9 MB on the first visit, and it's cached after that. Roughly 4.6 MB is
compiled code, 3.8 MB is game data (the monsters, objects, dungeon profiles,
the help files and four tilesets) and the rest is a little glue. A progress bar
runs while it downloads.

Savefiles
=========

Your game is saved in the browser, and it survives closing the tab.

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

There's only one terminal. The native builds can put messages, the inventory
and the monster list in windows of their own. A page has one canvas and no way
to ask for a second, so the browser version shows the main view alone. The
buttons that would have opened the others aren't there at all, so none of them
sits there doing nothing.

There's no fullscreen. The canvas already fills the window, and the browser
reserves Escape for leaving fullscreen. This game needs that key far too often
to lend it out.

All four tilesets are included, though: the original 8×8, Adam Bolt's, Nomad's
and David Gervais'. Between them they come to a little over two megabytes,
which is small enough for a page load, so they're all here. The game lists only
the tilesets it can find, so a build that leaves one out won't show it.

Everything else is as it is in the native builds: the same wilderness,
bestiary, lethality and savefile format.

Which browsers
==============

Any current Chrome, Edge, Firefox or Safari. WebAssembly has been in all of them
since 2017, and this build asks for nothing newer.

You'll need a keyboard, though. The game is driven by single keystrokes and has
no on-screen controls, so a phone or a tablet will load it and then leave you
with no way to play.

Which version
=============

The browser version follows the latest source, not the latest release: it's
rebuilt and republished whenever the game changes. That makes it
the newest ZangbandTK there is, and the least settled. The tagged builds on
:doc:`download` are the ones the :doc:`release log <releases>` describes.
