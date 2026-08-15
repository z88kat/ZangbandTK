"""
Reader and writer for Angband 4.2 ``lib/gamedata`` files.

4.2's format is ``key:value``, blank-line separated, with ``name:`` opening each
entry.  Several keys legitimately repeat (``flags``, ``blow``, ``friends``,
``drop``), so entries keep an ordered list of pairs rather than a dict.

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

from dataclasses import dataclass, field

from zformat import match_key


@dataclass
class Entry:
    """One ``name:``-introduced entry as ordered key/value pairs."""

    pairs: list[tuple[str, str]] = field(default_factory=list)
    lineno: int = 0

    @property
    def name(self) -> str:
        return self.get("name") or ""

    @property
    def key(self) -> str:
        return match_key(self.name)

    def get(self, key: str) -> str | None:
        for k, v in self.pairs:
            if k == key:
                return v
        return None

    def get_int(self, key: str) -> int | None:
        raw = self.get(key)
        try:
            return int(raw) if raw is not None else None
        except ValueError:
            return None

    def all(self, key: str) -> list[str]:
        return [v for k, v in self.pairs if k == key]

    def set(self, key: str, value) -> None:
        """Replace the first occurrence of `key`, or append if absent."""
        text = str(value)
        for i, (k, _) in enumerate(self.pairs):
            if k == key:
                self.pairs[i] = (k, text)
                return
        self.pairs.append((key, text))

    def render(self) -> str:
        return "\n".join(f"{k}:{v}" for k, v in self.pairs)


def parse(path: str) -> list[Entry]:
    """Read a 4.2 gamedata file into entries."""
    entries: list[Entry] = []
    current: Entry | None = None

    with open(path, encoding="utf-8", errors="replace") as handle:
        for lineno, raw in enumerate(handle, start=1):
            line = raw.rstrip("\n").rstrip("\r")
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            if ":" not in stripped:
                continue

            key, _, value = stripped.partition(":")
            if key == "name":
                current = Entry(lineno=lineno)
                entries.append(current)
            if current is not None:
                current.pairs.append((key, value))

    return entries


def header(path: str) -> str:
    """Leading comment block of a gamedata file, preserved verbatim on write."""
    out: list[str] = []
    with open(path, encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            line = raw.rstrip("\n")
            if line.startswith("#") or not line.strip():
                out.append(line)
            else:
                break
    return "\n".join(out).rstrip() + "\n"


def write(path: str, entries: list[Entry], preamble: str = "") -> None:
    """Write entries back in 4.2 format.

    Output is a pure function of `entries` — no timestamps, no ordering by hash
    — so that identical input yields an identical file (BAL-12).
    """
    with open(path, "w", encoding="utf-8") as handle:
        if preamble:
            handle.write(preamble)
            handle.write("\n")
        for entry in entries:
            handle.write(entry.render())
            handle.write("\n\n")


def index_by_key(entries: list[Entry]) -> dict[str, Entry]:
    """Map comparison key to entry; first occurrence wins on collision."""
    out: dict[str, Entry] = {}
    for entry in entries:
        out.setdefault(entry.key, entry)
    return out
