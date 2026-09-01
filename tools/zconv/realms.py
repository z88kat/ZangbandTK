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


def slice_for(pairs, cls: str, realm: str):
    """The 32 ``{level, mana}`` pairs for one class in one realm."""
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
            lines.append("book:%s book:%s:[%s]:%d:%s" % (
                realm, "town" if seen_book <= 2 else "dungeon",
                title, len(in_book), realm))
            lines.append("book-graphics:?:%s" % BOOK_COLOUR[realm])
            lines.append("book-properties:%d:%d:%d to 100" % (
                25 * seen_book, 40 - 8 * (seen_book - 1), seen_book * 10 - 9))

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
            lines.append("desc:%s" % entry.get("desc", "A working of " + realm))

    return lines


#: A colour per realm's books, matching object_base.txt.
BOOK_COLOUR = {
    "sorcery": "B", "chaos": "v", "trump": "w",
    "arcane": "R", "life": "G", "nature": "y", "death": "p",
}
