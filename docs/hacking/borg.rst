====
Borg
====

The Borg is an "Automatic Angband Player".

It was first written for about 2.8.0, separate from the game as
distributed. It was pulled into the game in around 3.3. It was removed
in 4.0 as there was too much conflict with the big changes made then.
It was reincorporated in around 4.2.3.

The name comes from "The Borg" in Star Trek which, in turn, comes from
cyborg.

The primary use of the Borg is entertainment. It is fun to watch the
Borg play the game, and it can be amusing to see how it handles
situations. It has also been used to test the game and find bugs.

ZangbandTK
==========

This file is upstream's, and everything below this section describes
*Angband* — its town, its bestiary, its single dungeon of one hundred levels
with Morgoth at the bottom. Most of it still applies. What follows is what
does not, and what this variant added, so that a reader is not misled by the
rest (DEC-17, BRG-21).

**The borg is test infrastructure here, not entertainment.** It exists because
there was no test in the repository that played the game: 112 unit suites test
rules in isolation and none of them walks a character out of a town, into a
dungeon and back. See ``.claude/plans/borg-development-plan.md`` for the
requirements, BRG-01 to BRG-22.

Running it without a keyboard
-----------------------------

The borg's only entry point upstream is ``^z`` then ``z``, through the UI, and
its only exit is a keypress. Neither is available to a test. The test front end
(``-mtest``, built when ``SUPPORT_TEST_FRONTEND`` is on) adds commands that
drive it headlessly::

   borg-seed [N]      seed the run; without N, reads ZTK_TEST_SEED
   borg-run N         play for N game turns, then hand control back
   borg-status?       one machine-readable line: turn, depth, maxdepth,
                      character level, hit points, armour, gold, deaths,
                      wielded weapon, and why it stopped
   borg-notes? [N]    the last N things the borg said
   borg-kills?        its target list, and whether any ally is on it
   borg-shops?        which shops it has found
   borg-mouths?       every dungeon mouth, its band, and whether it is
                      inside the current surface window
   borg-terrain?      what lies on the straight line to each deeper mouth
   borg-pet <race>    place an ally beside the player
   borg-pets?         how many pets survive, and how many turned hostile
   borg-roundtrip     save, reload and compare, mid-run
   borg-cheat L G     grant character level L and G gold (see below)
   borg-jump D        place the character at depth D, in a dungeon whose
                      band contains it

A character is born without the birth menus by setting ``ZTK_HEADLESS=1`` and
``ZTK_HEADLESS_CLASS``; the menus consume a number of keys that depends on the
roll, which is why key injection was not reproducible.

Two scripts wrap this. ``scripts/borg-smoke`` plays fixed seeds and exits
non-zero on a crash, an abort or a wedge, printing the seed to repeat it.
``scripts/borg-progress`` reports best depth and character level per class,
which is the regression signal BRG-18 asks for.

**A dead character is not a failure.** The borg reports death down the same
channel it uses for defects, and it dies often at low character level. Death
is reported as ``result=died`` with a zero exit; only crashes, aborts and
wedges fail.

**A run always ends.** As well as the turn budget there is a decision budget,
because a borg waiting for a prompt it cannot see makes decisions for ever
without the clock moving — which neither crashes nor aborts, it hangs, and a
hung CI job is a red build with no diagnosis.

What this variant changes underneath it
---------------------------------------

**Depth 0 is not a town.** It is a wilderness surface of 144 by 144 grids, a
window onto a world of roughly fourteen windows square, rebuilt and re-anchored
as the character crosses it. The borg's arrays were sized for Angband's
66-by-198 dungeon and it segfaulted on the first turn of every game; they are
now sized by a checked ceiling, and ``borg_init_cave()`` refuses to start if
the game's largest level does not fit.

**There are thirteen dungeons, not one.** Each has its own depth band, from the
Vaults of Amber at 1-15 to the Abyss at 90-127, and ``dungeon_get_next_level()``
*clamps* a descent to the dungeon's floor rather than refusing it — so a borg
that does not know about floors reads each clamp as a successful dive and loops
for ever. ``borg_prepared()`` now reports "no deeper in this dungeon".

Reaching a deeper dungeon means crossing the world, because no mouth is inside
the starting window and a town staircase always leads to the shallowest dungeon
there is. ``borg_flow_world()`` walks to a mouth, holding the destination in
*world* coordinates so it survives the window being rebuilt, and following the
roads — ``wild_place_roads()`` lays a spur to every mouth, so a road is
passable by construction and routes around the mountains.

**Seven magic realms replaced Angband's spell lists.** The borg casts through
ninety-five hardcoded ``borg_spell(ENUM)`` calls against Angband's
``enum borg_spells``, so **184 of this game's 224 realm spells cannot be cast
at all** — Nature has one. ``borg_best_spell_with_effect()`` and
``borg_spell_by_index()`` are the beginning of an answer; the rest is BRG-07.

**Monsters have three allegiances.** A pet is not a target: nothing that is not
hostile enters ``borg_kills[]``, which covers targeting, fear and pursuit
together.

**Several constants were sized for Angband and quietly outgrown** — the map
arrays, the spell ratings tables, ``book_idx``/``amt_book`` against a Mage's 28
books, and ``num_book`` indexed by an unbounded object sval. Each is now a
named ceiling with a startup check that refuses to run rather than corrupting
memory. If you add a constant here that mirrors something the game knows, give
it a guard.

The scoped route, and the cheats
--------------------------------

Standard play does not reach the depths this game's content lives at, and the
early-game grind is not what the borg exists to verify. The scoped route is a
single Warrior-Mage — all seven realms, so every spell and every source of pets
is reachable by one character — taken to depth 30 with four levers:
``borg-cheat`` grants character level and gold, ``borg-jump`` places it at a
dungeon using the map, and ``BORG_CHEAT_DEATH`` removes the attrition.

**The cheats remove attrition, not decisions.** The borg still chooses what to
buy, what to wield, what to cast, what to fight and when to descend, and every
level is generated and played. Grants happen at the start, never in reaction to
danger; the jump goes to a *dungeon*, not to the target depth. Deaths are
counted and reported even though they are cheated, because "reached depth 30,
died fourteen times" says something and the depth alone does not.

Running The Borg
================

It is not recommended to run the Borg on a live game, as it could
cause unexpected behavior or even crashes. It is best to run the Borg
on its own save file, or on a copy of your save file.

To run the Borg:

1. Ensure Angband is compiled with borg support (this is controlled by
   ``ALLOW_BORG``).  This is done by default in most distributions.
2. Start or load a game
3. Press ``^z`` (Ctrl-Z) to access the Borg command interface
4. Press ``z`` to activate the Borg
5. Watch the Borg play automatically
6. Press any key to stop the Borg when desired

Borg Command Interface
======================

The Borg command interface is only available when Angband is compiled
with borg support.

To access the Borg command interface, press ``^z`` (Ctrl-Z) during
gameplay. When you first run the command you'll be presented with a warning
message you can continue through. The most common command is ``z`` which
starts the Borg.

Pressing any key while the Borg is running will stop the Borg.

Main Commands
-------------

====== ========================================
``?``  Display Help
``c``  Toggle cheat death flag
``C``  List count of 'nasties'
``f``  Toggle flags
``F``  Fear levels of current location
``G``  Display selected grid Features
``h``  Borg_Has function
``I``  Display selected grid Information
``D``  Display selected grid Danger
``l``  Create a snapshot log file
``m``  Map information
``o``  Object Information
``p``  Borg Power
``P``  Level preparation information
``R``  Respawn Borg
``s``  Search mode
``S``  Dump spell info
``t``  Display targeting 
``u``  Update the Borg's variables (as if taking zero steps)
``v``  Version stamp
``w``  My Swap Weapon/Armor
``x``  Step the Borg
``y``  Last 75 steps
``z``  Activate the Borg
``!``  Time
``$``  Reload borg.txt
``@``  Borg LOS
``^``  Flow Pathway
``0``  Borg stats (str/int etc)
====== ========================================

Map Information
---------------

After pressing ``m`` from the main borg interface you enter map information 
display mode. This is map information as the borg understands it. The 
following selections can be made.

====== ========================================
``a``  Avoidances - dangerous areas with level of danger.
``f``  Features with subselection of which feature to show.
``g``  Glyph locations
``m``  Monsters
``o``  Objects
====== ========================================

Flag Commands
-------------

After pressing ``f`` from the main borg interface you enter flag toggle mode.
You will be able to select any borg configuration and change its runtime value.


Borg_has Commands
-----------------

After pressing ``h`` from the main borg interface you enter "has"
display mode. These are things the borg has. The list is put in the games
messages.

====== ========================================
``a``  Any
``i``  Inventory
``w``  Worn items
``r``  Artifacts 
``s``  Skills
====== ========================================


Search Mode
-------------

After pressing ``s`` from the main borg interface you enter a search string.
If the borg sees that string in the messages it will stop.  Default is 
"plain gold ring" for The One Ring.



Customizing The Borg
====================

The Borg's behavior is primarily configured through the ``borg.txt`` file.
This allows for extensive customization of the Borg's decision-making without
needing to recompile the game. If you do not have a ``borg.txt`` file a 
stripped down ``borg.txt`` will be generated when the borg is first started.

For source distributions a sample ``borg.txt`` file is provided in the 
``src/borg`` directory of the source code. To use it, copy this file to the 
user preferences directory for your operating system, and then customize it.

- Windows: Copy ``src/borg/borg.txt`` to ``lib/user/borg.txt``
- Linux\Unix: Copy ``src/borg/borg.txt`` to ``~/.angband/ZangbandTK/borg.txt``
- macOS with the Cocoa front end:: Copy ``src/borg/borg.txt`` to 
  ``Documents/ZangbandTK`` within your home directory 

If you are using a binary distribution of ZangbandTK, the default borg.txt file 
needs to copied from the spot where it was put in the distribution for that
platform.

- Windows: the ``borg.txt`` file is already in the user preferences directory.
- macOS with the Cocoa front end: ``borg.txt`` is included in the top level 
  directory of the dmg file from which you installed the game.
- Linux/Unix: the ``borg.txt`` may not have been included in the distribution
  or it might be in ``/usr/share/doc/angband/borg.txt``.  If it is not found it
  can be copied from the source distribution in ``src/borg/borg.txt`` or
  downloaded from github.

To download the ``borg.txt`` from github (https://github.com/angband/angband)
select the ``<> Code`` tab.  Then select the ``src`` directory, the ``borg``
subdirectory and download the ``borg.txt`` from there.  If you need an older 
version use the ``History`` link for that file to find the correct version.

Once copied, you can edit ``borg.txt`` to change the Borg's behavior. To apply
changes while the game is running, use the ``$`` command from the Borg command
interface (``^z``).

How you customize the Borg depends on whether you are using a pre-compiled
build or compiling from source.

Configuration Options
---------------------

The ``borg.txt`` file offers a wide range of options to customize the Borg's
behavior. Below is a summary of the key settings. For a complete list and
detailed explanations, refer to the comments within the ``borg.txt`` file
itself.

Adjusting Borg Speed
********************

When you first run the Borg it may move very slowly. This is often due to the
game's ``base delay factor``, a general setting that affects all animations. To
speed up the Borg you can decrease this value:

1. Press ``=`` to open the main options menu
2. Press ``d`` to change the ``delay factor``
3. Decrease the value

Conversely, if the Borg is moving too quickly to follow, you can increase this
value. You can also add a Borg-specific delay by setting ``borg_delay_factor``
in ``borg.txt``.

Worships
********
These settings (e.g., ``borg_worships_damage``, ``borg_worships_gold``)
influence the Borg's priorities and decision-making by assigning value to
different actions and items. For example, they can make the Borg favor
powerful weapons, seek out treasure, or prioritize speed.

Play Style
**********
- ``borg_plays_risky``: Makes the Borg dive deeper faster and be more
  aggressive in combat
- ``borg_kills_uniques``: Forces the Borg to defeat uniques before
  proceeding deeper into the dungeon

Item Management
***************
- ``borg_uses_swaps``: Allows the Borg to carry and use swap items for
  situational resistances and abilities
- ``borg_worships_gold``: Causes the Borg to return to town frequently to
  sell items for gold, especially at lower levels

Respawn and Continuous Play
***************************
- ``borg_cheat_death``: If enabled, the Borg will not die and will
  continue playing, enabling continuous play. This can be set in
  ``borg.txt`` or toggled via the Borg command interface (``^z``, then
  ``c``, then ``d``)
- ``borg_respawn_race`` and ``borg_respawn_class``: Specify the race and
  class for the next character when the Borg respawns
- ``borg_respawn_winners``: If enabled, the Borg will create a new
  character after defeating Morgoth

Dynamic Formulas
****************

The Borg can use either its internal hard-coded logic for decision-making
or a more flexible system of dynamic formulas defined in ``borg.txt``. To
enable the formula-based system, set the following in ``borg.txt``:

.. code-block:: ini

  borg_uses_dynamic_calcs = TRUE

The dynamic calculations are more customizable but may be slower and
are not always as up-to-date as the internal code logic.

Using Official Builds
---------------------

In most official builds, Borg support is already included and enabled. You just
need to copy and configure the ``borg.txt`` file in the correct location as
described above.

Compiling Yourself
------------------

When compiling from source, the Borg is enabled by default on most platforms.
For starter instructions on how to compile, see the :doc:`compiling` guide.

If you find the Borg is disabled in your build configuration, you can typically
enable it by:

- Uncommenting an ``allow_borg`` line in a configuration file (like
  ``config.h``)
- Passing a ``-DALLOW_BORG`` flag to the compiler

When compiling, you can also enable the ``SCORE_BORGS`` flag to allow Borg
characters to appear in the high score list. This is disabled by default.

Refer to the compilation instructions for your specific platform for details.
After compiling with Borg support, place your ``borg.txt`` file in the correct
location as described above.

Borg Logging
============

The Borg suppresses most messages by default. To see what the Borg is doing,
you'll want to use multi-window support to display additional information
windows.

Window Configuration
--------------------

For optimal Borg monitoring, open additional terminal windows to display:

- Equipment: See what the Borg is wearing and wielding
- Messages: View game messages and Borg status updates
- Monster Recall: See information about monsters the Borg encounters
- Inventory: Monitor what items the Borg is carrying

Set these up through the :ref:`window menu <showing-extra-info-in-subwindows>`
before activating the Borg. Borg-specific messages will appear in the
Messages window when verbose mode is enabled.

Verbose Mode
------------

Enable verbose mode to get detailed output about the Borg's decision-making
process, including calculations, target selection, danger assessment, and
action decisions.

Via Flag Command
****************

1. Press ``^z`` to access the Borg command interface
2. Press ``f`` to enter flag toggle mode
3. Select ``borg_verbose`` to toggle verbose mode on/off

Via Configuration
*****************

Set ``borg_verbose = TRUE`` in the ``borg.txt`` configuration file, then
reload with ``^z`` ``$``.

Log Snapshot
------------

Create a detailed snapshot of the current game state for debugging:

1. Press ``^z`` to access the Borg command interface
2. Press ``l`` to create a snapshot log file

This generates a comprehensive ``.map`` file (e.g., ``player_name.map``) in
your Angband ``archive`` directory containing:

- ASCII dungeon map: Current level layout showing terrain, monsters (``&``),
  items, and player (``@``) position
- Recent game messages: Last actions, movements, and events
- Complete character state: Equipment, inventory, quiver, and home contents
- Borg configuration: Current swap items and borg settings
- Detailed statistics: All internal borg trait values, resistances, and
  assessments

The snapshot provides a complete picture of both the game state and the
Borg's internal knowledge at that moment, useful for understanding its
behavior or debugging issues.

Borg Screensaver
================

The Borg can be configured to run as a Windows screensaver that
automatically plays the game in continuous play mode, automatically
restarting with new characters when the current character dies.

**WARNING:** The Angband display is not always dynamic. While modern LCD
monitors are not susceptible to burn-in, OLED displays may still experience
image retention with prolonged static content. Configure energy saving
settings to turn off your monitor after inactivity. The screensaver keeps
the processor and hard disk busy, preventing power-saving features that
depend on inactivity.

Installation
------------

1. Copy ``angband.scr`` and the included ``angband.ini`` into your Windows
   directory

2. Ensure you have the Windows version of Angband installed with all supporting
   files in the ``lib`` directory

3. Edit ``angband.ini`` with a text editor:
   
   - Set ``AngbandPath`` to point to your Angband installation directory
     (must end with a backslash ``\``)
   - Set ``SaverFile`` to the character name you want to use for the screensaver
     (a random character will be automatically created if the character doesn't
     exist)

   Example configuration::
   
       [Angband]
       AngbandPath="c:\games\angband-4.2.5\"
       SaverFile="Saver"

4. Test the screensaver in Windows Display Properties

It's recommended to create a normal character first using regular Angband,
set up your terminal windows as desired, save that file, and use that filename
as the ``SaverFile`` for your screensaver.

Technical Details
-----------------

- The screensaver is a renamed Windows Angband executable with modified
  ``main-win.c``
- Normal Borgs get highscore entries, but screensaver Borgs (continuous
  play mode) do not
- Uses low priority processing to avoid slowing down other processes

  - Can be toggled via "Options/Low priority" menu when using as normal
    executable for background Borg play
- Uses the normal Angband installation's ``angband.ini`` for screen layout,
  graphics, and sound settings
- Can be used as a normal Angband executable by renaming to ``angband.exe``

Known Limitations
-----------------

- No preview in Windows Display Properties
- Password protection not implemented
- Configuration requires manual ``ini`` file editing
- "Show scores" while Borg is running may cause crashes
- Cannot run the same savefile simultaneously (e.g., normal game 
  and screensaver)
- Info window sizes may increase when exiting pseudo-screensaver mode from
  options menu
