#!/usr/bin/env python3
"""
Make the command reference card, docs/downloads/zangbandtk-command-card.pdf.

Two sides of A4, landscape, three panels a side: every command the game has,
with its key in the original keyset and in the roguelike one.  After the
"Commands Reference Card" on pages 11 and 12 of Epyx's 1985 Rogue manual
(archive/books/rogueeypx.pdf), laid out for printing on one sheet, both
sides, and folding in three.

The keys are not typed in here.  Each row names a command by its description
in the game's own table, cmds_all[] in src/ui-game.c, as the Tk front end's
menus do (lib/tcl/commands.tcl), and the key in each keyset is read from that
table.  Movement, running and the rest of what lib/customize/pref.prf does
with keymaps can't be read the same way, so those rows are written out, and
--check holds each of them to the keymap it describes.

--check also fails if a command in the game is on no row of the card, or a
row names a command the game no longer has.  So a command added to the game
has to be given a place here, and the card has to be made again.

    scripts/command-card/build.py              make the PDF
    scripts/command-card/build.py --out FILE   make it somewhere else
    scripts/command-card/build.py --html FILE  write the page Chrome prints
    scripts/command-card/build.py --check      check the card against the
                                               game, and write nothing

Making the PDF needs Google Chrome or Chromium, which prints the page; set
CHROME to its path if it isn't found.  It runs headless with a profile of its
own, so it doesn't touch a browser that is open.  Chrome also lays the page
out first and says if a panel has more on it than fits, and the build fails
then, rather than print a card with its last lines cut off.  --check needs
only Python.

The fonts are IBM Plex: Plex Mono Bold for the keys, from lib/tcl/fonts, and
Plex Sans Condensed and Plex Serif, from fonts/ here.  All are under the SIL
Open Font License (fonts/IBMPlex-OFL.txt), and the PDF carries what it uses
of them.  They are kept out of lib/tcl/fonts because any font there is offered
in the Tk front end's Display settings.
"""

import argparse
import base64
import html
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
OUT = os.path.join(ROOT, "docs", "downloads", "zangbandtk-command-card.pdf")

# The lists in cmds_all[] that are keyed in the game's command loop.  The
# Dbg* lists are reached through ^A and are on the card as that one key.
LISTS = ["cmd_item", "cmd_action", "cmd_item_manage", "cmd_info",
         "cmd_util", "cmd_hidden"]


# --- what the game says

def read(path):
    with open(os.path.join(ROOT, path), encoding="utf-8") as f:
        return f.read()


def key_token(tok):
    """A key as src/ui-game.c spells it, as the card shows it."""
    m = re.fullmatch(r"KTRL\('(.)'\)", tok)
    if m:
        return "^" + m.group(1).upper()
    if tok == "KC_TAB":
        return "Tab"
    m = re.fullmatch(r"'(\\.|[^'\\])'", tok)
    if m:
        c = m.group(1)
        if c.startswith("\\"):
            c = {"\\'": "'", "\\\\": "\\", '\\"': '"'}[c]
        return c
    raise SystemExit("build.py: can't read the key %s in src/ui-game.c" % tok)


def game_commands():
    """{description: (original key, roguelike key)} from cmds_all[].

    cmd_init() gives a command with no roguelike key its original one, and so
    does this.
    """
    src = read("src/ui-game.c")
    commands = {}
    for name in LISTS:
        m = re.search(r"struct cmd_info %s\[\] =\s*\{(.*?)\n\};" % name,
                      src, re.S)
        if not m:
            raise SystemExit("build.py: no %s[] in src/ui-game.c" % name)
        body = re.sub(r"/\*.*?\*/", "", m.group(1), flags=re.S)
        for e in re.finditer(r'\{\s*"((?:[^"\\]|\\.)*)"\s*,\s*'
                             r"\{((?:'(?:\\.|[^'\\])'|[^}'])*)\}", body):
            desc = e.group(1).replace('\\"', '"')
            toks = re.findall(r"KTRL\('.'\)|KC_\w+|'(?:\\.|[^'\\])'",
                              e.group(2))
            keys = [key_token(t) for t in toks]
            if not keys:
                continue
            orig = keys[0]
            rogue = keys[1] if len(keys) > 1 else orig
            commands[desc] = (orig, rogue)
    return commands


def game_keymaps():
    """{(keyset, input): action} from lib/customize/pref.prf."""
    maps = {}
    act = None
    for line in read("lib/customize/pref.prf").splitlines():
        if line.startswith("keymap-act:"):
            act = line[len("keymap-act:"):]
        elif line.startswith("keymap-input:"):
            mode, inp = line[len("keymap-input:"):].split(":", 1)
            maps[(int(mode), inp)] = act
    return maps


def game_version():
    m = re.search(r'#define VERSION_STRING\s+"([^"]+)"', read("src/buildid.h"))
    return m.group(1)


# --- the card
#
# Each panel is a list of blocks:
#
#   ("h", title)                  a heading
#   (keys, text)                  a row, the same keys in both keysets
#   (orig keys, rogue keys, text) a row whose keys differ between them
#   ("note", text)                a line of small print
#   ("pads",)                     the two direction pads
#   ("title",)                    the card's title
#   ("legend",)                   what ^X and dir mean
#
# Keys are written in a little language:
#
#   {Look around}   the key the game gives that command, in that keyset
#   [Esc]           a key, as written
#   `@m1`           an inscription
#   dir             any direction key
#   +               the two are pressed together
#   anything else   a word in small print: or, then, click
#
# Row text is HTML, so & is &amp; and < is &lt;.

PANELS = [
    # Side one, left
    [
        ("title",),
        ("legend",),
        ("h", "Moving"),
        ("pads",),
        ("dir", "Step one square.  Into a monster attacks it, into a door "
                "opens it, into a known trap tries to disarm it."),
        ("Shift + dir", "Run until something interesting happens."),
        ("{Start running} dir", "Run, without Shift."),
        ("^dir", "Alter what's there without moving: attack, tunnel, open "
                 "or disarm."),
        ("{Alter a grid} dir", "Alter, without Ctrl."),
        ("{Walk} dir", "Step, picking up what you step onto."),
        ("{Walk into a trap} dir",
         "Step onto a trap without trying to disarm it."),
        ("{Stand still}",
         "Stay still for a turn, picking up what's here.  So does <b>5</b>."),
        ("{Go up staircase}", "Go up a staircase."),
        ("{Go down staircase}", "Go down a staircase.  With autoexplore on "
                                "and no stair here, both walk you to the "
                                "nearest known one."),
        ("{Start exploring}",
         "Walk to the nearest unknown ground.  Needs the autoexplore option."),
    ],
    # Side one, middle
    [
        ("h", "Resting and picking up"),
        ("{Rest for a while}",
         "Rest: <b>&amp;</b> as needed, <b>*</b> until HP and SP are both "
         "back, <b>!</b> until either is, or a number of turns."),
        ("{Pick up objects}", "Pick up what's on the floor here."),
        ("{Do autopickup}",
         "Pick up only what your inscriptions and options mark as yours."),
        ("h", "Doors, traps and rock"),
        ("{Open a door or a chest} dir",
         "Open a door or chest, picking any lock."),
        ("{Close a door} dir", "Close a door."),
        ("{Disarm a trap or chest} dir",
         "Disarm a trap or chest, or lock a door."),
        ("{Dig a tunnel} dir", "Tunnel through rock, rubble or a vein."),
        ("{Steal from a monster} dir", "Steal from a monster (rogues)."),
        ("h", "Fighting at range"),
        ("{Fire your missile weapon}", "Fire a missile from your launcher."),
        ("{Fire at nearest target}",
         "Fire your first missile at the nearest monster."),
        ("{Throw an item}", "Throw something."),
        ("{Target monster or location}",
         "Target a monster or a place.  See Targeting."),
        ("{Target closest monster}", "Target the closest monster."),
        ("h", "Spells and powers"),
        ("{Browse a book}", "Browse a book's spells."),
        ("{Gain new spells}", "Study a book to learn a new spell."),
        ("{Cast a spell}", "Cast a spell."),
        ("{Use a power}", "Use a racial, shape or psionic power."),
        ("{View abilities}",
         "What your race and class give you that isn't a spell or power."),
    ],
    # Side one, right
    [
        ("h", "Your pack"),
        ("{Display inventory listing}", "Inventory: what you carry."),
        ("{Display equipment listing}", "Equipment: what you wear and wield."),
        ("{Display quiver listing}", "Quiver: your missiles."),
        ("{Wear/wield an item}", "Wear or wield something."),
        ("{Take off/unwield an item}", "Take something off."),
        ("[x]", "&ndash;", "Wield the item inscribed <b>@0</b>: a weapon "
                           "swap."),
        ("{Drop an item}", "Drop something."),
        ("{Examine an item}",
         "Inspect an item: what it does, as far as you know."),
        ("{Inscribe an object}", "Inscribe an item.  See Inscriptions."),
        ("{Uninscribe an object}", "Remove an inscription."),
        ("{Ignore an item}", "Ignore an item, or every one like it."),
        ("{Toggle ignoring of items}", "Show or hide ignored items."),
        ("h", "Using things"),
        ("{Eat some food}", "Eat something."),
        ("{Quaff a potion}", "Quaff a potion."),
        ("{Read a scroll}", "Read a scroll."),
        ("{Fuel your light source}", "Refuel your lantern or torch."),
        ("{Aim a wand} dir", "Aim a wand."),
        ("{Use a staff}", "Use a staff."),
        ("{Zap a rod}", "Zap a rod."),
        ("{Activate an object}", "Activate something you're wearing."),
        ("{Use an item}", "Use any item: the game works out how."),
        ("h", "Special keys"),
        ("[Esc]", "Cancel a command or a prompt.  Clears "
                  "<b class='nb'>-more-</b>."),
        ("[0]", "Give the next command a count: <b>0</b>, the number, "
                "the key.  Any key stops it."),
        ("[^] key", "A control key in two strokes: <b>^</b> then <b>X</b> "
                    "is <b>^X</b>."),
        ("[\\] key", "Skip your keymaps: the next key is the game's own."),
    ],
    # Side two, left
    [
        ("h", "Looking around"),
        ("{Look around}",
         "Look at what's interesting nearby.  See Targeting."),
        ("{Full dungeon map}", "The whole level, shrunk to fit."),
        ("{Locate player on map}", "Scroll the map a section at a time."),
        ("{Center map}", "Centre the map on you."),
        ("{Display visible monster list}", "List the monsters you can see."),
        ("{Display visible item list}", "List the items you can see."),
        ("{Identify symbol}", "What a symbol on the map stands for."),
        ("{Repeat level feeling}", "Repeat the level feeling."),
        ("h", "You and your company"),
        ("{Character description}",
         "Character sheet.  Change your name or save it to a file from "
         "there."),
        ("{Check knowledge}",
         "What you know: objects, runes, artifacts, monsters, traps, shops, "
         "your history, the hall of fame."),
        ("{Things you have taken on}",
         "The quests you've taken on and not yet handed back."),
        ("{Command pets}",
         "Order your pets: how close to stay, doors, pick-ups, dismiss."),
        ("h", "In a shop or your home"),
        ("[p] or [g]", "Buy, or take from your home."),
        ("[s] or [d]",
         "Sell, or give it to be identified where nothing is sold; drop it "
         "at home."),
        ("[l] or [x]", "Examine the stock."),
        ("{Examine an item}", "Inspect something of yours."),
        ("[?]", "What the keys do here."),
        ("[Esc]", "Leave."),
        ("note", "Most of your own commands work in a shop too: "
                 "<b>{Display inventory listing}</b>, "
                 "<b>{Wear/wield an item}</b>, "
                 "<b>{Take off/unwield an item}</b>, "
                 "<b>{Inscribe an object}</b> and more."),
    ],
    # Side two, middle
    [
        ("h", "The game"),
        ("[Enter]", "A menu of every command, to choose from."),
        ("{Help}", "Help.  In the Tk front end, the manual."),
        ("{Interact with options}", "Options, keymaps, colours and visuals."),
        ("{Repeat previous command}",
         "Do the last command again, on the same item and target."),
        ("{Save and don't quit}", "Save and carry on."),
        ("{Save and quit}", "Save and quit."),
        ("{Retire character and quit}",
         "Retire the character.  It asks twice, and there's no way back."),
        ("{Save \"screen dump\"}", "Save the screen to a file, as HTML."),
        ("{Redraw the screen}", "Redraw the screen."),
        ("{Toggle windows}",
         "Switch subwindows between inventory and equipment."),
        ("{Load a single pref line}", "Type in one line of a pref file."),
        ("{Version info}", "Which version this is."),
        ("{Toggle wizard mode}",
         "Wizard mode, for testing.  Marks the character unscored for good."),
        ("{Debug mode commands}",
         "Debug commands, then a letter.  Asks first, and marks the "
         "character unscored."),
        ("{Borg commands}",
         "Hand the game to the borg, where it's built in.  Unscored too."),
        ("h", "Inscriptions"),
        ("note", "Written on an item with <b>{Inscribe an object}</b>.  "
                 "Each works with the key of the command it names."),
        ("`@m1`", "Choose it with <b>1</b> when that command asks: a "
                  "book for <b>m</b>, a wand for <b>a</b>, and so on."),
        ("`!q`", "Ask before using it with that command; <b>!*</b> with "
                 "any."),
        ("`^d`", "On something worn: ask before every <b>d</b>.  "
                 "<b>^*</b> asks before any command."),
        ("`=g`", "Always pick it up.  <b>=g4</b> stops at four."),
        ("`!!`", "Say when it has recharged."),
        ("`@v0`", "A throwing weapon goes in the quiver's first slot."),
    ],
    # Side two, right
    [
        ("h", "Messages"),
        ("{Show previous message}", "Show the last message again."),
        ("{Show previous messages}", "Message history."),
        ("{Take notes}", "Write a note in your character's history."),
        ("h", "Targeting"),
        ("note", "When a command asks for a direction:"),
        ("dir", "Fire or cast that way."),
        ("[']", "At the closest monster."),
        ("[5] or [t]", "At the current target."),
        ("[*]", "Choose a target, with the keys below."),
        ("note", "While choosing a target, or looking with "
                 "<b>{Look around}</b>:"),
        ("[Space] or [+] or [-]", "Next or previous interesting thing."),
        ("dir", "Move to the next one that way, or a square in free "
                "targeting."),
        ("[t] or [5] or [.]", "Target it and finish."),
        ("[r]", "Recall: more about the monster or item."),
        ("[Enter]", "The next thing in that square."),
        ("[o] or [p]", "Free targeting, from here or from you."),
        ("[m]", "Back to the interesting things."),
        ("[&lt;] [&gt;] [x]",
         "Nearest up stair, down stair, unknown ground."),
        ("[g]", "Walk there (not from a direction prompt)."),
        ("[q] or [Esc]", "Stop, choosing nothing."),
        ("h", "In the Tk front end"),
        ("click", "Step towards the pointer."),
        ("Shift + click", "Run that way."),
        ("double-click", "Walk to that square."),
        ("[F1]",
         "The manual on playing.  Each window's F1 opens its own page."),
        ("note", "The menus give each command's key in your keyset.  "
                 "Alt and the underlined letter (Option on the Mac) works "
                 "the buttons in a window."),
    ],
]

# Rows written out by hand, held to the keymaps in pref.prf: for each, the
# keyset (0 original, 1 roguelike), the input and the action it must have.
KEYMAP_CLAIMS = [
    # Digits, arrows and the keypad walk in both keysets; the letters in one.
    (0, "4", ";4"), (1, "4", ";4"), (0, "[Left]", ";4"), (1, "[Left]", ";4"),
    (0, "7", ";7"), (0, "[Home]", ";7"), (0, "{K}7", ";7"),
    (1, "h", ";4"), (1, "j", ";2"), (1, "k", ";8"), (1, "l", ";6"),
    (1, "y", ";7"), (1, "u", ";9"), (1, "b", ";1"), (1, "n", ";3"),
    # Shift and a direction runs: '.' then it in one keyset, ',' in the other.
    (0, "{S}[Left]", ".4"), (0, "{SK}4", ".4"),
    (1, "{S}[Left]", ",4"), (1, "{SK}4", ",4"), (1, "H", ",4"),
    # Control and a direction alters.
    (0, "^[Left]", "+4"), (1, "^[Left]", "+4"), (1, "^H", "+4"),
    # 5 stays still, by each keyset's own key for it.
    (0, "5", ","), (1, "5", "."),
    # x swaps weapons in the original keyset only.
    (0, "x", "w0"),
]

# What the pads show, as (original, roguelike, arrow) from top left.
PAD = [("7", "y", "&#8598;"), ("8", "k", "&#8593;"), ("9", "u", "&#8599;"),
       ("4", "h", "&#8592;"), ("5", None, ""), ("6", "l", "&#8594;"),
       ("1", "b", "&#8601;"), ("2", "j", "&#8595;"), ("3", "n", "&#8600;")]


def check(commands, keymaps):
    """Problems with the card, as a list of sentences."""
    problems = []
    used = set()
    for panel in PANELS:
        for block in panel:
            for part in block[:-1]:
                used.update(re.findall(r"\{([^}]+)\}", part))
            if block[0] == "note":
                used.update(re.findall(r"\{([^}]+)\}", block[1]))
    for name in sorted(used - set(commands)):
        problems.append('The card has "%s", which the game doesn\'t.' % name)
    for name in sorted(set(commands) - used):
        problems.append('The game has "%s" (%s), which the card doesn\'t.'
                        % (name, " / ".join(commands[name])))
    for mode, inp, act in KEYMAP_CLAIMS:
        got = keymaps.get((mode, inp))
        if got != act:
            problems.append("pref.prf maps %s in the %s keyset to %r, and the "
                            "card says %r." % (inp, ("original", "roguelike")
                                               [mode], got, act))
    # The pads: the roguelike letters, and 5 to stand still in both.
    for orig, rogue, _ in PAD:
        for mode in (0, 1):
            want = ";" + orig if orig != "5" else \
                commands["Stand still"][mode]
            if keymaps.get((mode, orig)) != want:
                problems.append("pref.prf doesn't map %s to %r in keyset %d."
                                % (orig, want, mode))
        if rogue and keymaps.get((1, rogue)) != ";" + orig:
            problems.append("pref.prf doesn't map %s to ;%s." % (rogue, orig))
    return problems


# --- the page

# Every table has the same three columns, so the text lines up down a panel.
COLS = '<colgroup><col class="ck"><col class="ck"><col></colgroup>'


def kbd(text):
    return "<kbd>%s</kbd>" % text


def keys_html(spec, mode, commands):
    out = []
    for tok in re.findall(r"\{[^}]+\}|\[[^\]]+\]|\S+", spec):
        if tok.startswith("{"):
            out.append(kbd(html.escape(commands[tok[1:-1]][mode])))
        elif tok.startswith("["):
            out.append(kbd(tok[1:-1]))
        elif tok.startswith("`"):
            out.append('<code>%s</code>' % tok[1:-1])
        elif tok == "dir":
            out.append('<span class="dir">dir</span>')
        elif tok == "+":
            out.append('<span class="plus">+</span>')
        elif tok == "&ndash;":
            out.append('<span class="none">&ndash;</span>')
        elif tok in ("Shift", "^dir"):
            if tok == "Shift":
                out.append(kbd("Shift"))
            else:
                out.append(kbd("^") + '<span class="dir">dir</span>')
        else:
            out.append('<span class="w">%s</span>' % tok)
    return " ".join(out)


def text_html(text, commands):
    """Row text: HTML, with {Command} put in as its key, or both keys."""
    def keys(m):
        orig, rogue = commands[m.group(1)]
        if orig == rogue:
            return html.escape(orig)
        return "%s</b> or <b>%s" % (html.escape(orig), html.escape(rogue))
    return re.sub(r"\{([^}]+)\}", keys, text)


def row_html(block, commands):
    if len(block) == 2:
        o = r = block[0]
    else:
        o, r = block[0], block[1]
    ko = keys_html(o, 0, commands)
    kr = keys_html(r, 1, commands)
    text = text_html(block[-1], commands)
    if ko == kr:
        return '<tr><td class="k both" colspan="2">%s</td><td class="d">%s' \
               '</td></tr>' % (ko, text)
    return '<tr class="differ"><td class="k">%s</td><td class="k r">%s</td>' \
           '<td class="d">%s</td></tr>' % (ko, kr, text)


def pads_html():
    def pad(mode):
        cells = []
        for orig, rogue, arrow in PAD:
            if mode == 0:
                label = orig
            else:
                label = rogue or "."
            cells.append('<div class="cell"><kbd>%s</kbd><i>%s</i></div>'
                         % (label, arrow))
        return '<div class="pad">%s</div>' % "".join(cells)

    return ('<div class="pads">'
            '<figure>%s<figcaption><b>Original</b><br>keypad, top-row '
            'digits, or arrows with Home, End, PgUp, PgDn for the '
            'diagonals</figcaption></figure>'
            '<figure>%s<figcaption><b>Roguelike</b><br>these letters, or '
            'any of the original\'s keys; <b>Shift</b> and a letter '
            'runs</figcaption></figure>'
            '</div>') % (pad(0), pad(1))


def title_html(version):
    return ('<header class="title">'
            '<h1>ZangbandTK</h1>'
            '<p class="sub">Command Reference Card</p>'
            '<p class="ver">Version %s &middot; zangbandtk.com</p>'
            '<p class="intro">Every command in the game.  Where the '
            '<b>roguelike keyset</b> uses a different key it has a column '
            'of its own, shaded; one key across both columns means both '
            'keysets use it.  Choose the keyset under <b>=</b>, User '
            'Interface Options.  Keys are case-sensitive: <kbd>T</kbd> is '
            'Shift and <kbd>t</kbd>.</p>'
            '</header>') % html.escape(version)


def legend_html():
    return ('<div class="legend">'
            '<span><kbd>^X</kbd> hold Ctrl and press X</span>'
            '<span><span class="dir">dir</span> a direction key</span>'
            '</div>')


def heads_html():
    return ('<table class="heads">' + COLS + '<tr><td class="k">Original</td>'
            '<td class="k r">Roguelike</td><td class="d"></td></tr></table>')


def font_face(family, path, weight, style):
    with open(path, "rb") as f:
        data = base64.b64encode(f.read()).decode("ascii")
    return ("@font-face { font-family: '%s'; font-weight: %d; "
            "font-style: %s; src: url(data:font/ttf;base64,%s) "
            "format('truetype'); }\n" % (family, weight, style, data))


def fonts_css():
    plex = os.path.join(HERE, "fonts")
    mono = os.path.join(ROOT, "lib", "tcl", "fonts")
    return "".join([
        font_face("Card Sans", os.path.join(
            plex, "IBMPlexSansCondensed-Regular.ttf"), 400, "normal"),
        font_face("Card Sans", os.path.join(
            plex, "IBMPlexSansCondensed-Italic.ttf"), 400, "italic"),
        font_face("Card Sans", os.path.join(
            plex, "IBMPlexSansCondensed-SemiBold.ttf"), 600, "normal"),
        font_face("Card Serif", os.path.join(
            plex, "IBMPlexSerif-SemiBold.ttf"), 600, "normal"),
        font_face("Card Serif", os.path.join(
            plex, "IBMPlexSerif-Italic.ttf"), 400, "italic"),
        font_face("Card Mono", os.path.join(
            mono, "IBMPlexMono-Bold.ttf"), 700, "normal"),
    ])


CSS = r"""
@page { size: 297mm 210mm; margin: 0; }
:root {
  --ink: #1f1b17;
  --muted: #6a6158;
  --accent: #8b1e1e;
  --rule: #d8cfc2;
  --cap: #f6f3ec;
  --capline: #8f877c;
  --rogue: #f1ece2;
}
* { box-sizing: border-box; }
html, body { margin: 0; padding: 0; background: #fff; }
body {
  font-family: 'Card Sans', sans-serif;
  font-size: 8.5pt;
  line-height: 1.24;
  color: var(--ink);
  -webkit-print-color-adjust: exact;
  print-color-adjust: exact;
}
.page {
  width: 297mm; height: 210mm;
  padding: 9mm 9mm 8mm;
  display: grid;
  grid-template-columns: 1fr 1fr 1fr;
  break-after: page;
  overflow: hidden;
}
.page:last-child { break-after: auto; }
.panel {
  height: 193mm;
  overflow: hidden;
  padding: 0 5mm;
  border-left: 0.25mm dashed var(--rule);
}
.panel:first-child { border-left: none; padding-left: 0; }
.panel:last-child { padding-right: 0; }

/* The title */
.title h1 {
  font-family: 'Card Serif', serif; font-weight: 600;
  font-size: 27pt; line-height: 1; margin: 0.5mm 0 0;
  letter-spacing: 0.01em; color: var(--ink);
}
.title .sub {
  font-family: 'Card Serif', serif; font-weight: 600;
  font-size: 10.5pt; letter-spacing: 0.2em; text-transform: uppercase;
  color: var(--accent); margin: 1.6mm 0 0;
  padding-bottom: 1.6mm; border-bottom: 0.5mm solid var(--accent);
}
.title .ver {
  font-family: 'Card Serif', serif; font-style: italic;
  color: var(--muted); margin: 1.2mm 0 0; font-size: 8pt;
}
.title .intro { margin: 2.2mm 0 0; }
.title kbd { font-size: 7pt; }

.legend {
  display: flex; gap: 4mm; flex-wrap: wrap;
  margin: 2.2mm 0 0; padding: 1.4mm 2mm;
  background: var(--cap); border-radius: 1mm; color: var(--muted);
}

/* Headings */
h2 {
  font-family: 'Card Serif', serif; font-weight: 600;
  font-size: 9.6pt; letter-spacing: 0.12em; text-transform: uppercase;
  color: var(--accent);
  margin: 4.2mm 0 1mm; padding-bottom: 0.7mm;
  border-bottom: 0.3mm solid var(--accent);
}
.panel > h2:first-child { margin-top: 0.5mm; }

/* The rows */
table { width: 100%; border-collapse: collapse; table-layout: fixed; }
col.ck { width: 15mm; }
td { vertical-align: top; padding: 0.55mm 0; }
td.k {
  text-align: center; white-space: nowrap;
  padding-left: 0.4mm; padding-right: 0.4mm;
}
td.k.r { background: var(--rogue); }
td.d { padding-left: 2.2mm; }
tr + tr td { border-top: 0.15mm solid #eee7dc; }
table.heads td {
  font-family: 'Card Serif', serif; font-style: italic;
  font-size: 7pt; color: var(--muted); padding: 0 0 0.6mm;
  border: none;
}
table.heads td.k.r {
  background: var(--rogue); border-radius: 0.8mm 0.8mm 0 0;
}

kbd {
  font-family: 'Card Mono', monospace; font-weight: 700;
  font-size: 7.6pt; line-height: 1.15;
  display: inline-block; min-width: 3.9mm;
  padding: 0.15mm 0.7mm 0.05mm;
  text-align: center;
  color: var(--ink);
  background: #fff;
  border: 0.2mm solid var(--capline);
  border-bottom-width: 0.55mm;
  border-radius: 0.8mm;
}
.dir {
  font-family: 'Card Serif', serif; font-style: italic;
  color: var(--accent); font-size: 8pt;
}
.w, .plus, .none { color: var(--muted); font-size: 7pt; }
code {
  font-family: 'Card Mono', monospace; font-weight: 700; font-size: 7.8pt;
  color: var(--accent);
}
.none { font-size: 8pt; }
.note {
  margin: 1.4mm 0 0.6mm; color: var(--muted); font-style: italic;
}
.note b { font-style: normal; color: var(--ink); }
.nb { white-space: nowrap; }

/* The direction pads */
.pads { display: flex; gap: 4mm; margin: 1.6mm 0 1.4mm; }
.pads figure {
  margin: 0; flex: 1; display: flex; gap: 2.4mm; align-items: center;
}
.pad {
  display: grid; grid-template-columns: repeat(3, 6.6mm);
  grid-auto-rows: 6.6mm; gap: 0.7mm; flex: none;
}
.pad .cell { position: relative; }
.pad kbd {
  width: 100%; height: 100%; min-width: 0;
  font-size: 8.4pt; padding-top: 1.1mm;
}
.pad i {
  position: absolute; right: 0.6mm; top: 0.1mm;
  font-style: normal; font-size: 5.5pt; color: var(--accent);
}
.pads figcaption { font-size: 6.9pt; color: var(--muted); line-height: 1.2; }
.pads figcaption b { color: var(--ink); }

.foot {
  margin-top: 3mm; padding-top: 1.2mm;
  border-top: 0.3mm solid var(--rule);
  font-size: 6.6pt; color: var(--muted);
}
"""

# Laid out, Chrome reports on each panel: how much room is left, or how much
# it is over.  The build reads that back from the DOM.
SCRIPT = r"""
document.fonts.ready.then(function () {
  var mm = 96 / 25.4, out = [];
  document.querySelectorAll('.panel').forEach(function (p, i) {
    var top = p.getBoundingClientRect().top, bottom = top;
    Array.prototype.forEach.call(p.children, function (c) {
      bottom = Math.max(bottom, c.getBoundingClientRect().bottom);
    });
    var spare = (p.clientHeight - (bottom - top)) / mm;
    out.push((i + 1) + ':' + spare.toFixed(1));
  });
  document.body.setAttribute('data-spare', out.join(' '));
});
"""


def page_html(commands, version):
    panels = []
    for n, panel in enumerate(PANELS):
        parts = []
        table = []

        def flush():
            if table:
                parts.append("<table>%s%s</table>" % (COLS, "".join(table)))
                del table[:]

        if n > 0:
            parts.append(heads_html())
        for block in panel:
            kind = block[0]
            if kind == "h":
                flush()
                parts.append("<h2>%s</h2>" % block[1])
            elif kind == "title":
                flush()
                parts.append(title_html(version))
            elif kind == "legend":
                flush()
                parts.append(legend_html())
                parts.append(heads_html())
            elif kind == "pads":
                flush()
                parts.append(pads_html())
            elif kind == "note":
                flush()
                parts.append('<p class="note">%s</p>'
                             % text_html(block[1], commands))
            else:
                table.append(row_html(block, commands))
        flush()
        if n == len(PANELS) - 1:
            parts.append('<p class="foot">Made from the game\'s own '
                         'command table and keymaps, as of version %s.  The '
                         'manual has the long version of every command, under '
                         'Command Descriptions, at zangbandtk.com.</p>'
                         % html.escape(version))
        panels.append('<section class="panel">%s</section>' % "".join(parts))

    pages = ['<div class="page">%s</div>' % "".join(panels[i:i + 3])
             for i in range(0, len(panels), 3)]
    return ("<!doctype html><html lang=\"en-GB\"><head>"
            "<meta charset=\"utf-8\">"
            "<title>ZangbandTK Command Reference Card</title>"
            "<style>%s%s</style></head><body>%s<script>%s</script>"
            "</body></html>") % (fonts_css(), CSS, "".join(pages), SCRIPT)


# --- printing it

def find_chrome():
    env = os.environ.get("CHROME")
    if env:
        return env
    for path in [
            "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome",
            "/Applications/Chromium.app/Contents/MacOS/Chromium"]:
        if os.path.exists(path):
            return path
    for name in ["google-chrome", "google-chrome-stable", "chromium",
                 "chromium-browser", "chrome"]:
        path = shutil.which(name)
        if path:
            return path
    raise SystemExit("build.py: no Chrome or Chromium found; set CHROME")


def chrome(args, profile):
    cmd = [find_chrome(), "--headless=new", "--disable-gpu", "--no-first-run",
           "--no-default-browser-check", "--disable-extensions",
           "--user-data-dir=" + profile] + args
    return subprocess.run(cmd, check=True, capture_output=True, text=True)


def spare_room(page, profile):
    """{panel: mm to spare, negative when it is over} as Chrome lays it out."""
    dom = chrome(["--virtual-time-budget=10000", "--window-size=1200,900",
                  "--dump-dom", "file://" + page], profile).stdout
    m = re.search(r'data-spare="([^"]*)"', dom)
    if not m:
        raise SystemExit("build.py: Chrome didn't lay the card out")
    return {int(a): float(b) for a, b in
            (p.split(":") for p in m.group(1).split())}


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0].strip())
    ap.add_argument("--out", default=OUT, help="the PDF to write")
    ap.add_argument("--html", help="write the page Chrome prints, and stop")
    ap.add_argument("--check", action="store_true",
                    help="check the card against the game; write nothing")
    args = ap.parse_args()

    commands = game_commands()
    problems = check(commands, game_keymaps())
    for p in problems:
        print("build.py: " + p, file=sys.stderr)
    if problems:
        return 1
    if args.check:
        print("The card has all %d of the game's commands." % len(commands))
        return 0

    page = page_html(commands, game_version())
    if args.html:
        with open(args.html, "w", encoding="utf-8") as f:
            f.write(page)
        return 0

    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "card.html")
        with open(path, "w", encoding="utf-8") as f:
            f.write(page)
        profile = os.path.join(tmp, "profile")
        spare = spare_room(path, profile)
        print("Room to spare, in mm: " + "  ".join(
            "%d: %.1f" % (k, v) for k, v in sorted(spare.items())))
        over = [k for k, v in spare.items() if v < 0]
        if over:
            print("build.py: panel %s has more on it than fits" %
                  ", ".join(map(str, over)), file=sys.stderr)
            return 1
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        chrome(["--no-pdf-header-footer", "--print-to-pdf-no-header",
                "--print-to-pdf=" + os.path.abspath(args.out),
                "file://" + path], profile)
    print("Wrote " + args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
