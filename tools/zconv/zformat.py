"""
Reader for Angband 2.8.1-era ``*_info.txt`` data files.

This is the format used by both Angband 2.8.1 and Zangband 2.7.5: line-oriented
records introduced by ``N:index:Name``, with subsequent single-letter tags
carrying colon-separated fields.  Field *meaning* differs per file, so this
module only tokenises; interpretation lives in the per-file readers below.

Copyright (c) 2026 ZangbandZK contributors

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

import re
from dataclasses import dataclass, field
from typing import Iterator


@dataclass
class Record:
    """One ``N:``-introduced entry, with its tagged lines in file order."""

    index: int
    name: str
    lines: list[tuple[str, str]] = field(default_factory=list)
    lineno: int = 0

    def first(self, tag: str) -> str | None:
        """Payload of the first line with `tag`, or None."""
        for t, v in self.lines:
            if t == tag:
                return v
        return None

    def all(self, tag: str) -> list[str]:
        """Payloads of every line with `tag`, in file order."""
        return [v for t, v in self.lines if t == tag]

    def flags(self) -> list[str]:
        """Flags from every ``F:`` line, split on ``|`` and de-duplicated.

        Order is preserved: flag order is not semantically meaningful, but
        stable output matters for BAL-12 (deterministic re-runs).
        """
        seen: dict[str, None] = {}
        for payload in self.all("F"):
            for tok in payload.split("|"):
                tok = tok.strip()
                if tok:
                    seen.setdefault(tok, None)
        return list(seen)


def parse(path: str) -> list[Record]:
    """Tokenise an ``*_info.txt`` file into records."""
    records: list[Record] = []
    current: Record | None = None

    with open(path, encoding="utf-8", errors="replace") as handle:
        for lineno, raw in enumerate(handle, start=1):
            line = raw.rstrip("\n").rstrip("\r")
            if not line or line.startswith("#"):
                continue
            if len(line) < 2 or line[1] != ":":
                continue

            tag, payload = line[0], line[2:]

            if tag == "N":
                idx, _, name = payload.partition(":")
                try:
                    index = int(idx)
                except ValueError:
                    continue
                current = Record(index=index, name=name.strip(), lineno=lineno)
                records.append(current)
            elif current is not None:
                current.lines.append((tag, payload))

    return records


# --- name normalisation -------------------------------------------------
#
# 2.8.1-era object names are written "& Long Sword~"; 4.2 writes "Long Sword~".
# Comparing the two without normalising inflates the Zangband-only object count
# from 135 to 166 (see phase1-content-and-flavour.md §1).

_ARTICLE = re.compile(r"^&\s*")


def normalise_name(name: str) -> str:
    """Strip the leading ``& `` article marker; keep the ``~`` plural marker."""
    return _ARTICLE.sub("", name).strip()


def match_key(name: str) -> str:
    """Key for comparing names across versions: normalised, plural-stripped, folded."""
    return normalise_name(name).replace("~", "").strip().casefold()


# --- per-file interpretation --------------------------------------------

_DICE = re.compile(r"^(\d+)d(\d+)$")


def parse_dice(spec: str) -> tuple[int, int] | None:
    """Parse ``NdM`` into (N, M), or None if not dice notation."""
    m = _DICE.match(spec.strip())
    return (int(m.group(1)), int(m.group(2))) if m else None


@dataclass
class Monster:
    """A monster race from ``r_info.txt``.

    Field layout, confirmed against 2.8.1's init1.c:
        ``I:speed:hitpoints:vision:armour_class:sleep``
        ``W:level:rarity:pad:experience``
        ``B:method:effect:damage``
    """

    index: int
    name: str
    speed: int | None = None
    hp_dice: tuple[int, int] | None = None
    vision: int | None = None
    ac: int | None = None
    sleep: int | None = None
    level: int | None = None
    rarity: int | None = None
    mexp: int | None = None
    blows: list[str] = field(default_factory=list)
    flags: list[str] = field(default_factory=list)
    desc: str = ""
    symbol: str | None = None
    colour: str | None = None
    lineno: int = 0

    @property
    def key(self) -> str:
        return match_key(self.name)


def _ints(payload: str) -> list[str]:
    return [p.strip() for p in payload.split(":")]


def read_monsters(path: str) -> list[Monster]:
    """Interpret ``r_info.txt`` records as monsters."""
    out: list[Monster] = []

    for rec in parse(path):
        mon = Monster(index=rec.index, name=rec.name, lineno=rec.lineno)

        if (g := rec.first("G")) is not None:
            parts = _ints(g)
            if len(parts) >= 2:
                mon.symbol, mon.colour = parts[0], parts[1]

        if (i := rec.first("I")) is not None:
            parts = _ints(i)
            if len(parts) >= 5:
                mon.speed = _int_or_none(parts[0])
                mon.hp_dice = parse_dice(parts[1])
                mon.vision = _int_or_none(parts[2])
                mon.ac = _int_or_none(parts[3])
                mon.sleep = _int_or_none(parts[4])

        if (w := rec.first("W")) is not None:
            parts = _ints(w)
            if len(parts) >= 4:
                mon.level = _int_or_none(parts[0])
                mon.rarity = _int_or_none(parts[1])
                mon.mexp = _int_or_none(parts[3])

        mon.blows = rec.all("B")
        mon.flags = rec.flags()
        mon.desc = " ".join(d.strip() for d in rec.all("D")).strip()
        out.append(mon)

    return out


def _int_or_none(text: str) -> int | None:
    try:
        return int(text)
    except (TypeError, ValueError):
        return None
