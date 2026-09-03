"""
Player spell conversion: Zangband ``magic_info[]`` -> Angband 4.2 spell lines.

Zangband keeps a player's spells in one array in ``tables.c`` rather than in a
data file: ``magic_info[MAX_CLASS]`` holds a ``{level, mana}`` pair for every
class, realm and spell it has, and the names live in a second array beside it.
4.2 wants ``spell:Name:level:mana:fail:exp`` under a ``book:`` in ``class.txt``,
so two of the four numbers have to be derived.  Neither is invented; both come
out of formulas the two games share (BAL-08, and the conversion rules recorded
in the M9 section of the development plan):

``sfail``
    Zangband has no per-spell failure figure and computes one at cast time.
    The two formulas are structurally identical -- same level-difference term,
    same stat adjustment, same five-per-point penalty for casting short of
    mana, same floor-then-cap order -- and differ only in that Zangband derives
    the base from the spell's level where 4.2 stores it.  So
    ``sfail = slevel + 20`` for Arcane and ``slevel * 3 // 2 + 20`` elsewhere.

``sexp``
    ``5 * book**2`` -- 5, 20, 45, 80 -- because Zangband awards
    ``5 * book**2 * slevel`` and 4.2 awards ``sexp * slevel`` (DEC-51).

**The table is sliced by position, not by comment.** It holds exactly
11 classes x 7 realms x 32 spells = 2,464 pairs.  Two classes, Chaos-Warrior
and High-Mage, have no ``/*** Name ***/`` comment, so parsing by those comments
finds nine classes and silently mis-assigns the rest.  Position is checked
against an independent table instead: a class-realm pair Zangband forbids holds
32 unusable entries, and one it allows holds usable ones.

Copyright (c) 2026 ZangbandTK contributors

This work is free software; you can redistribute it and/or modify it under the
terms of either:

a) the GNU General Public License as published by the Free Software
   Foundation, version 2, or

b) the "Angband licence":
   This software may be copied and distributed for educational, research,
   and not for profit purposes provided that this copyright and statement
   are included in all such copies.  Other copyrights may also apply.
"""

from __future__ import annotations

import pathlib
import re
import tomllib

ROOT = pathlib.Path(__file__).resolve().parents[2]

# The order the table is laid out in.  Both lists are positional and neither is
# alphabetical; changing either silently mis-slices everything after it.
CLASSES = ["Warrior", "Mage", "Priest", "Rogue", "Ranger", "Paladin",
           "Warrior-Mage", "Chaos-Warrior", "Monk", "Mindcrafter", "High-Mage"]
REALMS = ["life", "sorcery", "nature", "chaos", "death", "trump", "arcane"]

SPELLS_PER_REALM = 32
SPELLS_PER_BOOK = 8

#: A level of 99 is Zangband's "this class cannot learn this".
UNUSABLE = 99


def _table_body(src: str) -> str:
    start = src.index("player_magic magic_info[MAX_CLASS] =")
    return src[start:src.index("\n};", start)]


def read_pairs(src: str) -> list[tuple[int, int]]:
    """Every ``{level, mana}`` in the table, in declaration order."""
    pairs = [(int(a), int(b)) for a, b in
             re.findall(r"\{\s*(\d+)\s*,\s*(\d+)\s*\}", _table_body(src))]

    want = len(CLASSES) * len(REALMS) * SPELLS_PER_REALM
    if len(pairs) != want:
        raise ValueError(
            f"magic_info holds {len(pairs)} pairs, expected {want}. "
            "The table's shape has changed and slicing by position is no "
            "longer safe.")
    return pairs


class NotAZangbandClass(KeyError):
    """4.2 has a casting class Zangband never did.

    Druid, Necromancer and Blackguard are Angband 4.2's own, so ``magic_info[]``
    holds no level or mana figures for them.  That is a gap in the *source*, not
    a bug here, and it decides which realms can be imported directly: Arcane
    (Mage, Rogue) and Life (Paladin, Priest) can, Nature is half-affected
    through the Druid, and Death cannot be imported at all -- both classes that
    carry it, Necromancer and Blackguard, post-date Zangband.
    """


# DEC-55: whose figures a class that Zangband never had borrows.
#
# Three of 4.2's nine casting classes post-date Zangband, so `magic_info[]` has
# no row for them. Each takes the figures of the Zangband class it matches on
# `spell_first`, `spell_weight` and casting stat -- the three constants both
# games inherited from the same ancestor, and which six of the nine match
# exactly and by name. See DEC-55 for the derivation and for why the two that
# are not unique resolve the way they do.
#
# A donor is one word here on purpose: if a class plays wrong at these figures,
# changing it and re-running the converter is the whole of the fix.
DONORS = {
    "Druid": "Priest",              # 1/350/WIS, unique among Nature's carriers
    "Necromancer": "Mage",          # ties with High-Mage; generalist wins
    "Blackguard": "Paladin",        # 1/600 matches nothing; nearest weight
}


def donor_for(cls: str) -> str:
    """The Zangband class whose figures `cls` uses -- itself, or its donor."""
    return DONORS.get(cls, cls)


def slice_for(pairs, cls: str, realm: str):
    """The 32 ``{level, mana}`` pairs for one class in one realm."""
    cls = donor_for(cls)
    if cls not in CLASSES:
        raise NotAZangbandClass(
            f"{cls} is Angband 4.2's own class; Zangband's magic_info[] has no "
            f"figures for it, and DEC-55 names no donor. Known classes: "
            f"{', '.join(CLASSES)}."
        )

    base = (CLASSES.index(cls) * len(REALMS) + REALMS.index(realm)) \
        * SPELLS_PER_REALM
    return pairs[base:base + SPELLS_PER_REALM]


def usable(entries) -> bool:
    return any(level < UNUSABLE for level, _ in entries)


def read_names(src: str, realm: str) -> list[str]:
    """The 32 spell names for a realm, in the same order as its pairs.

    The comment that opens each block is not written the same way twice.  Six
    realms are introduced by ``/* Common <Realm> Spellbooks */``; Arcane, which
    has no rare books, is introduced by
    ``/* Arcane Spellbooks (_only_ common spells) */``.  Both forms are
    accepted, and the "Common" one is preferred where it exists so that Trump,
    which carries an outer ``/* Trump Spellbooks */`` heading as well, starts in
    the right place.
    """
    title = realm.capitalize()
    start = src.find(f"/* Common {title} Spellbooks */")
    if start < 0:
        m = re.search(rf"/\* {title} Spellbooks[^*]*\*/", src)
        if not m:
            raise ValueError(f"no spell-name block found for {realm}.")
        start = m.start()

    # The block closes with "\n\t}" -- with a comma for the six realms that
    # have a realm after them, and without one for Arcane, which is last.
    names = re.findall(r'"([^"]+)"', src[start:src.index("\n\t}", start)])

    if len(names) != SPELLS_PER_REALM:
        raise ValueError(
            f"{realm} lists {len(names)} spell names, expected "
            f"{SPELLS_PER_REALM}.")
    return names


def base_fail(level: int, realm: str) -> int:
    """4.2's stored ``sfail``, from the base Zangband derives at cast time."""
    return level + 20 if realm == "arcane" else level * 3 // 2 + 20


def book_exp(book: int) -> int:
    """``sexp`` for a spell in book 1-4 (DEC-51)."""
    return 5 * book * book


def convert(src: str, cls: str, realm: str):
    """One class's spells in one realm, as dicts ready for a ``spell:`` line.

    Empty when the class cannot learn the realm, which is checked before the
    names are read: a Warrior has no spells in any realm, and asking for its
    Arcane list should say so rather than fail looking one up.
    """
    pairs = read_pairs(src)
    entries = slice_for(pairs, cls, realm)
    if not usable(entries):
        return []

    names = read_names(src, realm)
    out = []

    for i, (name, (level, mana)) in enumerate(zip(names, entries)):
        if level >= UNUSABLE:
            continue
        book = i // SPELLS_PER_BOOK + 1
        out.append({
            "index": i,
            "book": book,
            "name": name,
            "level": level,
            "mana": mana,
            "fail": base_fail(level, realm),
            "exp": book_exp(book),
        })

    return out


def entitlements(src: str) -> dict[str, list[str]]:
    """Which realms each class may take, read from the table itself.

    Zangband states this twice -- here, and in ``realm_choices1/2[]`` -- and the
    two agreeing is what makes slicing by position trustworthy.
    """
    pairs = read_pairs(src)
    return {cls: [r for r in REALMS if usable(slice_for(pairs, cls, r))]
            for cls in CLASSES}


def read_realmmap() -> dict:
    """The effect chains, book names and dispositions, from realmmap.toml."""
    path = pathlib.Path(__file__).resolve().parent / "realmmap.toml"
    with path.open("rb") as handle:
        return tomllib.load(handle)


def check_coverage(src: str, realm: str, spellmap: dict) -> list[str]:
    """Complaints about realmmap.toml disagreeing with the spell table.

    A name misspelt in realmmap.toml is silent otherwise: the spell is emitted
    with no effect chain and looks like a deferral. This makes it loud.
    """
    real = read_names(src, realm)
    mapped = spellmap.get(realm, {}).get("spells", {})
    out = []
    for name in real:
        if name not in mapped:
            out.append("no realmmap entry for %r" % name)
    for name in mapped:
        if name not in real:
            out.append("realmmap has %r, which is not a %s spell" % (name, realm))
    data = spellmap.get(realm, {})
    books = data.get("books", [])
    want = SPELLS_PER_REALM // SPELLS_PER_BOOK
    if len(books) != want:
        out.append("%s has %d book titles, wanted %d"
                   % (realm, len(books), want))
    tiers = data.get("book-tiers")
    if tiers is not None and len(tiers) != want:
        out.append("%s has %d book-tiers, wanted %d" % (realm, len(tiers), want))
    in_town = data.get("books-in-town", BOOKS_IN_TOWN)
    if not 0 <= in_town <= want:
        out.append("%s puts %d of %d books in town" % (realm, in_town, want))
    return out


def book_noun(realm: str) -> str:
    """The object kind a realm's books are, read from ``realm.txt``.

    Not ``f"{realm} book"``.  That is right for Sorcery and Chaos by
    coincidence -- their realms happen to be named after their books -- and
    wrong for every realm 4.2 already had: Life's books are *prayer books*,
    Arcane's are *magic books*.  Emitting ``life book`` would name an object
    kind that does not exist, and the parse would fail on a realm this was
    never run against.
    """
    text = (ROOT / "lib" / "gamedata" / "realm.txt").read_text()
    block = re.search(rf"^name:{realm}$(.*?)(?=^name:|\Z)", text,
                      re.M | re.S)
    if not block:
        raise ValueError(f"realm.txt has no record for {realm}.")

    noun = re.search(r"^book-noun:(.+)$", block.group(1), re.M)
    if not noun:
        raise ValueError(f"realm.txt gives {realm} no book-noun.")
    return noun.group(1).strip()


#: Every key an entry in realmmap.toml may carry.  Anything else is a mistake
#: rather than a comment, and is refused: a `desc2` key that this file simply
#: ignored cost a silent half-description on eight Life spells before it was
#: noticed, which is the same shape of bug as a mutation whose flag was skipped.
ENTRY_KEYS = {"line", "effects", "desc", "defer", "note"}


def check_entry_keys(spellmap: dict) -> list[str]:
    """Complaints about realmmap keys nothing reads."""
    out = []
    for realm, data in spellmap.items():
        for name, entry in data.get("spells", {}).items():
            for key in entry:
                if key not in ENTRY_KEYS:
                    out.append(f"{realm}/{name}: nothing reads key {key!r}"
                               f" (known: {', '.join(sorted(ENTRY_KEYS))})")
    return out


def class_blocks(gamedata: str) -> dict[str, str]:
    """class.txt split into one text block per class, keyed by name."""
    out = {}
    for blk in re.split(r"(?m)^(?=name:)", gamedata)[1:]:
        out[blk.split("\n")[0][len("name:"):].strip()] = blk
    return out


#: A `book:` line in class.txt, which is not the same as a `book-graphics:` or
#: `book-properties:` line.  Written as "anything starting `book:`" rather than
#: as a pattern for the tval, which is what it was until Trump: every realm
#: before it had a book-noun ending in the word "book", and matching on that
#: made Trump's *deck* invisible -- so the realm before it in the file ran on
#: through Trump's books and the checker reported a class that had been emitted
#: correctly as broken.
BOOK_LINE = re.compile(r"(?m)^book:")


def extract_realm_block(block: str, realm: str) -> list[str]:
    """The lines class.txt holds for one realm inside one class, or [].

    A realm's books are contiguous and in order -- that is what makes appending
    a realm safe -- so the block runs from its first `book:` line to the line
    before the next book of a different realm, or the end of the class.
    """
    starts = [m.start() for m in
              re.finditer(BOOK_LINE, block)]
    for i, at in enumerate(starts):
        line = block[at:block.index("\n", at)]
        if not line.endswith(":" + realm):
            continue
        # Found the realm's first book; run to the first later book of another.
        end = len(block)
        for later in starts[i + 1:]:
            other = block[later:block.index("\n", later)]
            if not other.endswith(":" + realm):
                end = later
                break
        lines = block[at:end].rstrip("\n").split("\n")

        # The block ends at its last `desc:`. What follows -- a blank line, or
        # the banner comment that separates one class from the next -- belongs
        # to the file rather than to the realm, and swallowing it moves the next
        # class's heading into the middle of this one's spells.
        while lines and (not lines[-1].strip()
                         or lines[-1].lstrip().startswith("#")):
            lines.pop()

        return lines
    return []


# The directives `emit_books()` produces. Anything else inside a realm's book
# region belongs to the class rather than the realm -- the Druid's
# `equip:nature book:[...]` has to sit after the `book:` line that creates that
# object kind, or the game will not start -- so the reproduction check compares
# what the generator owns and reports the rest.
BOOK_GRAMMAR = ("book:", "book-graphics:", "book-properties:", "spell:",
                "effect:", "effect-yx:", "effect-msg:", "dice:", "expr:",
                "desc:")


def generated_only(lines: list[str]) -> list[str]:
    """Just the lines the generator is responsible for."""
    return [l for l in lines if l.strip() and l.startswith(BOOK_GRAMMAR)]


def check_all(src: str, spellmap: dict, gamedata: str) -> list[str]:
    """Complaints about realmmap.toml, the source table and class.txt agreeing.

    Three ways a realm's content can be wrong, and this is all three:

    * a name in realmmap.toml that the source table does not have, or the
      reverse. Sorcery's *Teleport* was keyed as "Teleport Self", so the
      converter found no entry and emitted the spell with its level, mana and
      failure and **no effect** -- indistinguishable from a deliberate deferral,
      and it shipped in 3.55.0 doing nothing;
    * a class.txt block that no longer matches what the converter produces,
      which is what a hand edit looks like from here;
    * a realm emitted into a class the entitlement table does not give it, or
      missing from one it does.

    Returns (complaints, notes). Complaints fail the build; notes are things
    worth printing that are not wrong -- a class directive living inside a
    realm's book region, for one. Meant to be run by the build gate rather than
    by hand, because noticing is what failed last time.
    """
    out = []
    notes = []
    blocks = class_blocks(gamedata)

    for realm in spellmap:
        out.extend("%s: %s" % (realm, c) for c in
                   check_coverage(src, realm, spellmap))

        # Which classes class.txt says carry this realm, and which the
        # entitlement table says can.
        holds = sorted(cls for cls, blk in blocks.items()
                       if extract_realm_block(blk, realm))
        for cls in holds:
            if donor_for(cls) not in CLASSES:
                out.append("%s: %s holds it, Zangband never had that class, and "
                           "DEC-55 names no donor" % (realm, cls))
                continue
            want = emit_books(src, cls, realm, spellmap)
            if not want:
                out.append("%s: %s holds it but the table gives it none"
                           % (realm, cls))
                continue
            raw = extract_realm_block(blocks[cls], realm)
            have = generated_only(raw)
            strays = [l for l in raw if l not in have and l.strip()]
            for stray in strays:
                notes.append("%s: %s keeps %s inside the book region; not "
                             "generated, so not compared"
                             % (realm, cls, stray.strip()))
            if have == want:
                continue
            out.append("%s: %s does not match the converter (%d lines in the "
                       "file, %d from the converter)"
                       % (realm, cls, len(have), len(want)))
            for n, (a, b) in enumerate(zip(have, want)):
                if a != b:
                    out.append("    first difference at line %d" % n)
                    out.append("      file:      %s" % a)
                    out.append("      converter: %s" % b)
                    break

    return out, notes


def emit_books(src: str, cls: str, realm: str, spellmap: dict) -> list[str]:
    """One class's realm as the `book:`/`spell:` lines class.txt wants.

    The numbers come from `magic_info[]` and the effects from realmmap.toml, and
    keeping those apart is the point: a spell is described once and priced once
    per class, so the Mage's Sorcery and the Rogue's Sorcery are the same spells
    at different levels rather than two transcriptions that can drift.

    A deferred spell is emitted with its level, mana and failure and no effect
    chain. That is deliberate -- see realmmap.toml -- and the caller reports it.
    """
    spells = convert(src, cls, realm)
    if not spells:
        return []

    realm_data = spellmap[realm]
    lines: list[str] = []
    seen_book = None

    for s in spells:
        if s["book"] != seen_book:
            seen_book = s["book"]
            in_book = [x for x in spells if x["book"] == seen_book]
            title = realm_data["books"][seen_book - 1]
            in_town = realm_data.get("books-in-town", BOOKS_IN_TOWN)
            tiers = realm_data.get("book-tiers", BOOK_TIERS)
            lines.append("book:%s:%s:[%s]:%d:%s" % (
                book_noun(realm), "town" if seen_book <= in_town else "dungeon",
                title, len(in_book), realm))
            lines.append("book-graphics:?:%s" % BOOK_COLOUR[realm])
            lines.append("book-properties:%s" % tiers[seen_book - 1])

        entry = realm_data["spells"].get(s["name"], {})
        lines.append("spell:%s:%d:%d:%d:%d" % (
            s["name"], s["level"], s["mana"], s["fail"], s["exp"]))

        for item in entry.get("effects", []):
            if item.startswith(("dice:", "expr:", "effect-yx:", "effect-msg:")):
                lines.append(item)
            else:
                lines.append("effect:" + item)

        if "defer" in entry:
            lines.append("desc:This working is beyond what this game can yet")
            lines.append("desc: express, and does nothing.  See realmmap.toml.")
        else:
            # A description may be one string or a list of them.  4.2 joins
            # consecutive `desc:` lines without adding a space, so every line
            # after the first carries its own leading space, exactly as the
            # hand-written spells in class.txt do.
            text = entry.get("desc", "A working of " + realm)
            parts = [text] if isinstance(text, str) else list(text)
            for n, part in enumerate(parts):
                lines.append("desc:%s%s" % ("" if n == 0 else " ", part))

    return lines


#: A colour per realm's books, matching object_base.txt.
# Cost, weight and allocation depth, one line per book tier.
#
# Not Zangband's own numbers. Zangband prices its four books 100 / 1000 / 25000
# / 100000 and allocates the rare two at depths 65 and 95 on a 128-level
# dungeon, and neither scale transfers: 4.2 stops at 100 and its gold is not
# Zangband's gold. What does transfer is the shape -- two books you buy in town
# and two you find deep -- so these are 4.2's own figures for the same shape,
# lifted from the Mage's five books (tiers 1, 2, 4 and 5 of
# 25/400/1600/30000/50000). A new realm's books then price and appear at the
# same depths as the books already in the game, which is the point.
BOOK_TIERS = (
    "25:40:1 to 100",
    "400:40:10 to 100",
    "30000:15:50 to 100",
    "50000:10:75 to 100",
)

# How many of a realm's four books can be bought in town, by default.
#
# The flag is not cosmetic: `store.c` always stocks a town book and never a
# dungeon one, and `init.c` gives a dungeon book the ignore-element flags and
# marks it good. A realm may override both this and the tiers above, and
# Arcane does -- see realmmap.toml.
BOOKS_IN_TOWN = 2

BOOK_COLOUR = {
    "sorcery": "B", "chaos": "v", "trump": "w",
    "arcane": "R", "life": "G", "nature": "y", "death": "p",
}
