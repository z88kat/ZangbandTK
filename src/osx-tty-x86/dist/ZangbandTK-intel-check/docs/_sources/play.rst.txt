===================
Play in the browser
===================

The game runs in a browser, with nothing to install and nothing to download by
hand: `zangbandtk.com/play <https://zangbandtk.com/play/>`_.

It is the real game — the same C, compiled to WebAssembly rather than to a
machine's own instructions. Nothing is emulated and nothing runs on a server:
the browser executes the compiled code directly, the same way it would run any
other program it was handed. The wilderness is generated in the tab you have
open.

What it costs to start
======================

About 9 MB on the first visit, cached afterwards: roughly 4.6 MB of compiled
code, 3.8 MB of game data — the monsters, objects, dungeon profiles, the help
files and four tilesets — and a little glue. A progress bar runs while it
arrives.

Savefiles
=========

Your game is saved in the browser, and it survives closing the tab.

It is worth being clear about what that means, because it is not a file you can
find. The savefile lives in the browser's own storage, which makes it specific to
one browser on one machine: a character rolled in Chrome is not there in Firefox,
nor on your phone, and clearing site data for the domain deletes it along with
everything else the site has stored. Nothing is uploaded anywhere, so nothing can
be recovered if it goes.

If a character matters to you, play a native build from :doc:`download`, where
the savefile is a file on your disk that you can copy.

What is missing
===============

The browser version is deliberately smaller than the native ones.

**No sound.** Compiled out rather than switched off, which also keeps 3 MB of
samples out of the download.

**One terminal.** The native builds can put messages, the inventory and the
monster list in windows of their own. A page has one canvas and no way to ask for
a second, so the browser version shows the main view alone. The buttons that
would have opened the others are not there rather than being there and doing
nothing.

**No fullscreen.** The canvas already fills the window, and the browser reserves
Escape for leaving fullscreen — which is a key this game needs far too often to
lend out.

**All four tilesets.** The original 8×8, Adam Bolt's, Nomad's and David
Gervais', which between them are a little over two megabytes — small enough for
a page load, which is why they are all here. The game offers only the tilesets
it can actually find, so a build that leaves one out does not list it.

Everything else is the whole game: the same wilderness, the same bestiary, the
same lethality, the same savefile format.

Which browsers
==============

Any current Chrome, Edge, Firefox or Safari. WebAssembly has been in all of them
since 2017, and this build asks for nothing newer.

A keyboard, though. The game is driven by single keystrokes and has no on-screen
controls, so a phone or a tablet will load it and then leave you with no way to
play it.

Which version
=============

The browser version tracks the latest source, not the latest release: it is
rebuilt and republished whenever the game changes. That makes it the newest
ZangbandTK there is, and the least settled. The tagged builds on
:doc:`download` are the ones the :doc:`release log <releases>` describes.
