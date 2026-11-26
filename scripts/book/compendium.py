#!/usr/bin/env python3
"""
The book's treasure list and bestiary, written from the game itself.

build.py calls chapters() to get two chapters it puts after the manual's own
Objects and Monsters:

- The Treasure List: every kind of object, the weapons and armour in tables,
  everything else in a paragraph each saying what it does; then every
  artifact.
- The Bestiary: a full entry for each of the 219 uniques, the House of Amber
  first, and a table of every other monster.

The words are the game's.  The game's spoiler front end describes each
object, artifact and monster as it would to a player who knew everything
about it, so a game built with that front end (SUPPORT_SPOIL_FRONTEND in
CMake, --enable-spoil with configure) is run once to write them:

    angband -mspoil -duser=TMP -- -O objects -a artifacts -M monsters

and what they don't say is read from lib/gamedata: an artifact's story, a
bow's multiplier, the range a ring's bonus is rolled from, which monsters
belong together.  The spoilers are written for a character, so what depends
on one -- "your chance of success is 91.2%" -- is taken out.

Run on its own it writes the two chapters as a page to look at:

    scripts/book/compendium.py --game PATH/angband --html FILE
"""

import argparse
import html
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
GAMEDATA = os.path.join(ROOT, "lib", "gamedata")

TREASURE, BESTIARY = "treasure-list", "bestiary"
TITLES = {TREASURE: "The Treasure List", BESTIARY: "The Bestiary"}


# --- reading the game's data

def records(*names):
    """The records of lib/gamedata files, each a list of (field, value)."""
    out = []
    for name in names:
        path = os.path.join(GAMEDATA, name)
        if not os.path.exists(path):
            continue
        rec = None
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.rstrip("\n")
                if not line or line.startswith("#") or ":" not in line:
                    continue
                field, _, value = line.partition(":")
                if field == "name":
                    rec = [("name", value)]
                    out.append(rec)
                elif rec is not None:
                    rec.append((field, value))
    return out


def first(rec, field, default=""):
    for f, v in rec:
        if f == field:
            return v
    return default


def every(rec, field):
    return [v for f, v in rec if f == field]


def joined_desc(rec):
    """A record's desc: lines joined the way the game joins them."""
    return "".join(every(rec, "desc")).strip()


def words(text):
    return re.sub(r"\s+", " ", text).strip()


# --- running the game

def run_spoilers(game, tmp):
    """Write the three spoilers with GAME; return {kind: text}."""
    files = {"O": "objects.txt", "a": "artifacts.txt", "M": "monsters.txt"}
    user = os.path.join(tmp, "spoil-user")
    os.makedirs(user, exist_ok=True)
    args = [os.path.abspath(game), "-mspoil", "-duser=" + user, "--"]
    for opt, name in files.items():
        args += ["-" + opt, os.path.join(tmp, name)]
    # The game finds lib/ beside itself, as a build's game/ directory has.
    proc = subprocess.run(args, cwd=os.path.dirname(os.path.abspath(game)),
                          capture_output=True, text=True, timeout=300)
    out = {}
    for opt, name in files.items():
        path = os.path.join(tmp, name)
        if not os.path.exists(path):
            sys.exit("compendium.py: %s wrote no %s spoiler; is it built with "
                     "the spoiler front end, from this source?\n%s"
                     % (game, name, proc.stdout + proc.stderr))
        with open(path, encoding="utf-8", errors="replace") as f:
            out[opt] = f.read()
    return out


# --- tidying the game's words for a book

def paragraphs(text):
    """Wrapped text as a list of paragraphs, each one line."""
    paras, cur = [], []
    for line in text.split("\n"):
        if line.strip():
            cur.append(line.strip())
        elif cur:
            paras.append(" ".join(cur))
            cur = []
    if cur:
        paras.append(" ".join(cur))
    return paras


def no_player(text):
    """Take out what the spoiler says about the character it imagined."""
    text = re.sub(r",? which your device skill increases by [\d.]+% for an "
                  r"average of [\d.]+( damage| hitpoints)?", "", text)
    text = re.sub(r"\s*Your chance of success is [\d.]+%\.?", "", text)
    return text.strip()


# Lines that say only what an object is hurt by: true, but they would be on
# nearly every entry and say little.
HURT = re.compile(r"^(Can be destroyed by|Cannot be harmed by) [^.]*\.$")


def clean_lines(paras):
    out = []
    for p in paras:
        lines = [s for s in re.split(r"(?<=\.)\s+(?=[A-Z+-])", p)
                 if not HURT.match(s.strip())]
        p = no_player(" ".join(lines))
        if p:
            out.append(p)
    return out


def esc(text):
    return html.escape(text, quote=False)


def number(n):
    return "{:,}".format(int(n))


def anchor(text):
    return re.sub(r"[^a-z0-9]+", "-", text.lower()).strip("-")


# --- objects

def kind_name(name):
    """object.txt's "& Long Bow~" as "Long Bow"."""
    name = name.replace("& ", "").replace("~", "")
    name = re.sub(r"\|([^|]*)\|[^|]*\|", r"\1", name)
    return name.strip()


def roll_range(expr):
    """The least and most a value such as 1+M5 or 2d4 can come to."""
    lo = hi = 0
    for term in re.findall(r"[+-]?[^+-]+", expr.replace(" ", "")):
        sign = -1 if term.startswith("-") else 1
        term = term.lstrip("+-")
        m = re.fullmatch(r"(\d*)d(\d+)", term)
        if m:
            n = int(m.group(1) or 1)
            a, b = n, n * int(m.group(2))
        elif re.fullmatch(r"M\d+", term):
            a, b = 0, int(term[1:])
        elif term.isdigit():
            a = b = int(term)
        else:
            return None
        if sign < 0:
            a, b = -b, -a
        lo, hi = lo + a, hi + b
    return lo, hi


def property_names():
    names = {}
    for rec in records("object_property.txt"):
        code = first(rec, "code")
        if code:
            names[code] = first(rec, "name")
    return names


def object_data():
    """object.txt by (type, name): bow multipliers, rolled modifiers."""
    data = {}
    for rec in records("object.txt", "object.zangband.txt"):
        data[(first(rec, "type"), first(rec, "name"))] = rec
    return data


def rolled_bonuses(rec, prop_names):
    """For a kind whose bonuses are rolled, what they can come to."""
    out = []
    for values in every(rec, "values"):
        for item in values.split("|"):
            m = re.match(r"\s*([A-Z_]+)\[([^\]]+)\]", item)
            if not m or m.group(1).startswith("RES_"):
                continue
            r = roll_range(m.group(2))
            if not r or r[0] == r[1]:
                continue
            name = prop_names.get(m.group(1), m.group(1).lower())
            out.append("%+d to %+d %s" % (r[0], r[1], name))
    return out


def parse_objects(text):
    kinds = []
    for m in re.finditer(r"^@kind ([^\n]*)\n(.*?)^@end$", text, re.M | re.S):
        f = m.group(1).split("|")
        kinds.append({
            "group": f[0], "tval": f[1], "name": f[2],
            "level": int(f[3]), "cost": int(f[4]), "weight": int(f[5]),
            "dice": f[6], "ac": int(f[7]), "depths": f[8],
            "text": m.group(2)})
    return kinds


def weight(tenths):
    return "%d.%d" % (tenths // 10, tenths % 10)


def depths(d):
    return d.replace("-", "&ndash;") if d else "&ndash;"


def table(head, rows, cls="list"):
    out = ['<table class="%s"><thead><tr>' % cls]
    out += ["<th>%s</th>" % h for h in head]
    out.append("</tr></thead><tbody>")
    for row in rows:
        out.append("<tr>" + "".join("<td>%s</td>" % c for c in row)
                   + "</tr>")
    out.append("</tbody></table>")
    return "".join(out)


def section(doc, title, body, level=2):
    return ('<section id="%s--%s"><h%d>%s</h%d>%s</section>'
            % (doc, anchor(title), level, esc(title), level, body))


# What each tval is called in the tables of weapons and armour.
TVAL_TITLES = [
    ("shot", "Shots"), ("arrow", "Arrows"), ("bolt", "Bolts"),
    ("sword", "Swords and daggers"), ("polearm", "Polearms and axes"),
    ("hafted", "Blunt weapons"), ("digging", "Diggers"),
    ("soft armor", "Soft armour"), ("hard armor", "Hard armour"),
    ("dragon armor", "Dragon armour"), ("cloak", "Cloaks"),
    ("shield", "Shields"), ("helm", "Helms"), ("crown", "Crowns"),
    ("gloves", "Gloves"), ("boots", "Boots"),
]

ENTRY_GROUPS = [
    ("Lights and fuel", "Light"),
    ("Amulets", "Amulets"), ("Rings", "Rings"),
    ("Potions", "Potions"), ("Scrolls", "Scrolls"),
    ("Food", "Food"), ("Mushrooms", "Mushrooms"),
    ("Wands", "Wands"), ("Staffs", "Staffs"), ("Rods", "Rods"),
    ("Chests", "Chests"), ("Figurines", "Figurines"),
    ("Statues", "Statues"),
]


def treasure(spoil_objects, spoil_artifacts):
    kinds = parse_objects(spoil_objects)
    data = object_data()
    props = property_names()
    doc = TREASURE
    parts = ['<h1>%s</h1>' % TITLES[doc], (
        "<p>Every kind of object in the game, and every artifact, as a sage "
        "who knew all about them would describe them. <i>Level</i> is the "
        "object's own; <i>depths</i> are the levels it is found on; "
        "<i>cost</i> is what it is worth, in gold, before a shop adds its "
        "price and before any enchantment it comes with. Weights are in "
        "pounds.</p>")]

    def by(tval):
        return sorted((k for k in kinds if k["tval"] == tval),
                      key=lambda k: (k["level"], k["cost"], k["name"]))

    # Weapons
    body = []
    rows = []
    for k in by("bow"):
        rec = data.get((k["tval"], k["name"]), [])
        shoots = {"SHOOTS_SHOTS": "shots", "SHOOTS_ARROWS": "arrows",
                  "SHOOTS_BOLTS": "bolts"}
        flags = " ".join(every(rec, "flags"))
        what = next((v for f, v in shoots.items() if f in flags), "")
        rows.append([esc(kind_name(k["name"])), "&times;%s"
                     % first(rec, "pval", "?"), what, weight(k["weight"]),
                     k["level"], depths(k["depths"]), number(k["cost"])])
    body.append(section(doc, "Bows", table(
        ["Bow", "Mult.", "Shoots", "Wt", "Lvl", "Depths", "Cost"], rows), 3))
    for tval, title in TVAL_TITLES[:7]:
        rows = [[esc(kind_name(k["name"])), k["dice"], weight(k["weight"]),
                 k["level"], depths(k["depths"]), number(k["cost"])]
                for k in by(tval)]
        if rows:
            body.append(section(doc, title, table(
                ["", "Dice", "Wt", "Lvl", "Depths", "Cost"], rows), 3))
    parts.append(section(doc, "Weapons", "".join(body)))

    # Armour
    body = []
    for tval, title in TVAL_TITLES[7:]:
        rows = [[esc(kind_name(k["name"])), k["ac"], weight(k["weight"]),
                 k["level"], depths(k["depths"]), number(k["cost"])]
                for k in by(tval)]
        if rows:
            body.append(section(doc, title, table(
                ["", "AC", "Wt", "Lvl", "Depths", "Cost"], rows), 3))
    parts.append(section(doc, "Armour", "".join(body)))

    # Everything else, a paragraph each
    for group, title in ENTRY_GROUPS:
        entries = []
        for k in sorted((k for k in kinds if k["group"] == group),
                        key=lambda k: (k["level"], k["cost"], k["name"])):
            rec = data.get((k["tval"], k["name"]), [])
            paras = clean_lines(paragraphs(k["text"]))
            rolled = rolled_bonuses(rec, props)
            if rolled:
                # The spoiler shows one roll; the book shows the range.
                paras = [re.sub(r"^([+-]\d+ [a-z][a-z ,]*\.\s*)+", "", p)
                         for p in paras]
                paras = ["%s." % "; ".join(rolled)] + [p for p in paras if p]
            name = kind_name(k["name"])
            if k["tval"] == "flask":
                name = "Flask of oil"
            facts = "Level %d, depths %s, %s gold." % (
                k["level"], depths(k["depths"]), number(k["cost"]))
            text = " ".join(esc(p) for p in paras) or "No other effect."
            entries.append('<p class="entry"><b>%s.</b> <span class="facts">'
                           '%s</span> %s</p>' % (esc(name), facts, text))
        if entries:
            parts.append(section(doc, title, "".join(entries)))

    parts.append(artifacts(spoil_artifacts, data))
    return ('<section class="chapter compendium" id="%s--" data-doc="%s">%s'
            '</section>\n' % (doc, doc, "".join(parts)))


def artifacts(text, objects):
    doc = TREASURE
    stories = {}
    for rec in records("artifact.txt", "artifact.zangband.txt"):
        d = joined_desc(rec)
        if d and d != "An artifact of Zangband.":
            stories[first(rec, "name")] = d
    kind_descs = {words(joined_desc(rec)) for rec in objects.values()
                  if joined_desc(rec)}

    groups, cur = [], None
    lines = text.split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]
        nxt = lines[i + 1] if i + 1 < len(lines) else ""
        if line.strip() and re.fullmatch(r"=+", nxt.strip() or "x") and \
                not line.startswith("Artifact Spoilers"):
            cur = (line.strip(), [])
            groups.append(cur)
            i += 2
            continue
        if cur and line.strip() and re.fullmatch(r"-+", nxt.strip() or "x"):
            j = i + 2
            body = []
            while j < len(lines) and not (
                    j + 1 < len(lines) and lines[j].strip() and
                    re.fullmatch(r"[-=]+", lines[j + 1].strip() or "x")):
                body.append(lines[j])
                j += 1
            cur[1].append((line.strip(), "\n".join(body)))
            i = j
            continue
        i += 1

    out = ['<p>An artifact is one of a kind: once it has been made, in your '
           'game, it is never made again. Each is listed with its depths, '
           'how often it is made where it can be (its <i>rarity</i>, the '
           'higher the commoner) and its weight.</p>']
    for title, arts in groups:
        entries = []
        for head, body in arts:
            paras = paragraphs(body)
            footer = ""
            for p in list(paras):
                m = re.match(r"Min Level (\d+), Max Level (\d+), Generation "
                             r"chance (\d+), Power \d+, ([\d.]+) lbs", p)
                if m:
                    footer = ("Depths %s&ndash;%s, rarity %s, %s lb."
                              % m.groups())
                    paras.remove(p)
            paras = [p for p in paras if words(p) not in kind_descs]
            paras = clean_lines(paras)
            name = re.sub(r"^the ", "", head)
            story = next((s for n, s in stories.items() if n in head), "")
            m = re.match(r"(.*?)((?: [(\[<][^)\]>]*[)\]>])*)$", name)
            title_part, stats = m.group(1), m.group(2).strip()
            entries.append(
                '<div class="artifact"><p class="entry"><b>%s</b> %s '
                '<span class="facts">%s</span></p>%s%s</div>'
                % (esc(title_part), esc(stats), footer,
                   '<p class="story">%s</p>' % esc(story) if story else "",
                   "".join("<p>%s</p>" % esc(p) for p in paras)))
        out.append(section(doc, title, "".join(entries), 3))
    return section(doc, "Artifacts", "".join(out))


# --- monsters

def parse_monsters(text):
    """The extended monster spoiler, by name: (stats line, body)."""
    out = {}
    blocks = re.split(r"\n(?=\S.*\(.*'.'\)\n=== )", text)
    for b in blocks:
        head, _, rest = b.partition("\n")
        stats, _, body = rest.partition("\n")
        m = re.match(r"(\[[UQ]\] )?(.*?)\s+\([^()]*'.'\)$", head.strip())
        if m and stats.startswith("=== "):
            out[m.group(2)] = body
    return out


def monster_text(body, desc):
    """The spoiler's account of a monster, without its story and without
    what depends on the character it imagined."""
    text = words(body)
    d = words(desc)
    if d and text.startswith(d):
        text = text[len(d):].strip()
    text = re.sub(r"\((\d+d\d+), \d+%\)", r"(\1)", text)
    text = re.sub(r" \(\d+%\)", "", text)
    text = re.sub(r",? averaging [\d,]+ damage on each of (his|her|its|their)"
                  r" turns", "", text)
    return text


# Monsters the game has and the book leaves out (the owner's ruling, 26
# November 2026).  Sauron and Morgoth are Angband's ending, and the ending
# here is Amber's (quest.txt); the Cthulhu Mythos came in with Zangband.
# Both are the drift away from Zelazny that this project treats as a defect,
# so the book doesn't show them, though the game still has them.  Not every
# tentacled thing is Lovecraft's: the ghast and Shuma-Gorath are D&D's and
# Marvel's, the shambler and the dweller on the threshold are neither, and
# they stay.
LEFT_OUT = {
    # Angband's ending
    "Sauron, the Sorcerer", "Wolf-Sauron", "Serpent-Sauron",
    "Vampire-Sauron", "Morgoth, Lord of Darkness",
    # The Mythos
    "rat-thing", "servant of glaaki", "sand-dweller", "moon beast", "mi-go",
    "serpent man", "xiclotlan", "gnoph-keh", "shantak", "deep one", "dhole",
    "gug", "Bokrug", "leng spider", "star vampire", "elder thing",
    "flying polyp", "shan", "Father Dagon", "Mother Hydra", "nightgaunt",
    "fire vampire", "chthonian", "spawn of ubbo-sathla",
    "Ubbo-Sathla, the Unbegotten Source", "byakhee",
    "formless spawn of tsathoggua", "hunting horror",
    "dark young of shub-niggurath", "colour out of space",
    "Chaugnar Faugn, Horror from the Hills", "lloigor",
    "Quachil Uttaus, Treader of the Dust", "shoggoth", "Zoth-Ommog",
    "Yibb-Tstll the Patient One", "Ghatanothoa", "Zhar the Twin Obscenity",
    "Eihort, the Thing in the Labyrinth", "The King in Yellow",
    "Shudde M'ell", "Hastur the Unspeakable",
    "Nyogtha, the Thing that Should not Be", "Ahtu, Avatar of Nyarlathotep",
    "Daoloth, the Render of the Veils", "Hypnos, Lord of Sleep", "Glaaki",
    "Rhan-Tegoth", "Tsathoggua, the Sleeper of N'kai",
    "Atlach-Nacha, the Spider God", "Y'golonac", "Yig, Father of Serpents",
    "Cthugha, the Living Flame", "Cyaegha", "Ithaqua the Windwalker",
    "Abhoth, Source of Uncleanness", "star-spawn of cthulhu",
    "Tulzscha, the Green Flame", "Yog-Sothoth, the All-in-One",
    "Shub-Niggurath, Black Goat of the Woods",
    "Nodens, Lord of the Great Abyss", "Nyarlathotep, the Crawling Chaos",
    "Azathoth, Seething Nuclear Chaos", "Great Cthulhu",
    "hound of Tindalos",
}


def bestiary(spoil_monsters):
    doc = BESTIARY
    spoil = parse_monsters(spoil_monsters)
    bases = {first(r, "name"): r for r in records("monster_base.txt")}
    mons = []
    left_out = set()
    for rec in records("monster.txt", "monster.zangband.txt",
                       "monster.zangbandtk.txt"):
        name = first(rec, "name")
        if name.startswith("<"):
            continue
        if name in LEFT_OUT:
            left_out.add(name)
            continue
        flags = " ".join(every(rec, "flags"))
        base = first(rec, "base")
        mons.append({
            "name": name, "base": base,
            "glyph": first(rec, "glyph") or first(bases.get(base, []),
                                                  "glyph"),
            "level": int(first(rec, "depth", "0")),
            "speed": int(first(rec, "speed", "110")) - 110,
            "hp": int(first(rec, "hit-points", "1")),
            "ac": int(first(rec, "armor-class", "0")),
            "exp": int(first(rec, "experience", "0")),
            "unique": "UNIQUE" in flags.split(),
            "amber": "AMBERITE" in flags.split(),
            "desc": joined_desc(rec),
        })
    # A name here that the game no longer has is a list gone stale.
    stale = LEFT_OUT - left_out
    if stale:
        raise SystemExit("compendium.py: LEFT_OUT names monsters the game "
                         "doesn't have: %s" % ", ".join(sorted(stale)))
    # The game's own name for a monster, which can differ in case
    spoil_by_lower = {k.lower(): v for k, v in spoil.items()}

    def entry(m):
        body = spoil_by_lower.get(m["name"].lower(), "")
        account = monster_text(body, m["desc"]) if body else ""
        stats = ("Level %d &middot; speed %+d &middot; %s hit points "
                 "&middot; AC %d &middot; %s exp" % (
                     m["level"], m["speed"], number(m["hp"]), m["ac"],
                     number(m["exp"])))
        return ('<div class="monster"><p class="mhead"><b>%s</b> '
                '<span class="glyph">%s</span></p><p class="facts">%s</p>'
                '%s%s</div>' % (
                    esc(m["name"]), esc(m["glyph"]), stats,
                    '<p class="story">%s</p>' % esc(m["desc"])
                    if m["desc"] else "",
                    "<p>%s</p>" % esc(account) if account else ""))

    def order(m):
        return (m["level"], m["name"])

    amber = sorted((m for m in mons if m["amber"]), key=order)
    uniques = sorted((m for m in mons if m["unique"] and not m["amber"]),
                     key=order)
    rest = [m for m in mons if not m["unique"]]

    parts = ['<h1>%s</h1>' % TITLES[doc], (
        "<p>The monsters of the game. The uniques, each of them one of a "
        "kind, have an entry apiece: the House of Amber first, then the rest "
        "in the order you are likely to meet them. Every other monster is "
        "in the tables at the end, under its kind.</p><p>An entry gives the "
        "level a monster belongs to, its speed against a normal speed of "
        "+0, its hit points, its armour class and the experience it is "
        "worth (divided by your level when you kill it). The account after "
        "the story is the game's own, as monster recall gives it once you "
        "know everything there is to know.</p><p>The game also has Sauron, "
        "Morgoth and the monsters of the Cthulhu Mythos, from the games it "
        "grew out of. This book leaves them out.</p>")]
    parts.append(section(doc, "The House of Amber", (
        "<p>Oberon's blood, deep in the dungeon and meant to be met late. "
        "Killing one may bring down the blood curse of Amber on you.</p>"
        + "".join(entry(m) for m in amber))))
    parts.append(section(doc, "The uniques",
                         "".join(entry(m) for m in uniques)))

    groups = {}
    for m in rest:
        groups.setdefault(m["base"], []).append(m)
    tables = []
    for base in sorted(groups, key=lambda b: words(
            first(bases.get(b, []), "desc", b)).lower()):
        title = first(bases.get(base, []), "desc", base) or base
        rows = [[esc(m["name"]), m["level"], "%+d" % m["speed"],
                 number(m["hp"]), m["ac"], number(m["exp"])]
                for m in sorted(groups[base], key=order)]
        tables.append(section(doc, "%s (%s)" % (
            title, groups[base][0]["glyph"]), table(
                ["", "Lvl", "Spd", "HP", "AC", "Exp"], rows), 3))
    parts.append(section(doc, "Every other monster", (
        "<p>By kind, and within a kind by level.</p>" + "".join(tables))))
    return ('<section class="chapter compendium" id="%s--" data-doc="%s">%s'
            '</section>\n' % (doc, doc, "".join(parts)))


# --- for build.py

def chapters(game, tmp):
    """{doc: chapter html} for the treasure list and the bestiary."""
    spoil = run_spoilers(game, tmp)
    return {TREASURE: treasure(spoil["O"], spoil["a"]),
            BESTIARY: bestiary(spoil["M"])}


def main():
    ap = argparse.ArgumentParser(description=__doc__.strip().split("\n\n")[0])
    ap.add_argument("--game", required=True,
                    help="a game built with the spoiler front end")
    ap.add_argument("--html", required=True, help="the page to write")
    args = ap.parse_args()
    with tempfile.TemporaryDirectory() as tmp:
        ch = chapters(args.game, tmp)
    with open(args.html, "w", encoding="utf-8") as f:
        f.write("<!doctype html><meta charset=utf-8>"
                + ch[TREASURE] + ch[BESTIARY])
    print("Wrote %s" % args.html)


if __name__ == "__main__":
    main()
