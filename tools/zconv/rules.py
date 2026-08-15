"""
Conversion rules, each traceable to a numbered requirement.

Every rule returns a :class:`Value` carrying the requirement that produced it
and a confidence level, so the review report (BAL-11) can state not just what
the tool chose but why, and which numbers it had to invent.

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

import statistics
from dataclasses import dataclass
from typing import Any

# Confidence levels, weakest last.  Anything below CONVERTED needs human review.
EXACT = "exact"          # same meaning, same scale; taken unchanged
CONVERTED = "converted"  # transformed by a documented, verified rule
DERIVED = "derived"      # computed from 4.2's own curve
INVENTED = "invented"    # no basis in either source; a guess

ORDER = {EXACT: 0, CONVERTED: 1, DERIVED: 2, INVENTED: 3}


@dataclass
class Value:
    value: Any
    rule: str
    confidence: str
    note: str = ""


# --- scale conversions ---------------------------------------------------


def experience(zang_mexp: int | None, level: int | None) -> Value | None:
    """BAL-02 — Zangband stores *total* experience; 4.2 stores per-monster-level.

    Zangband's award is ``mexp / plev`` (xtra2.c, exp_for_kill); 2.8.1 and 4.2
    both use ``mexp * level / plev``.  Importing without dividing inflates the
    award by roughly the monster's depth — about 20x on average.
    """
    if zang_mexp is None:
        return None
    if not level:
        # Depth-0 monsters: 4.2's formula yields zero regardless, so the
        # stored value cannot be recovered by division.
        return Value(zang_mexp, "BAL-02", INVENTED,
                     "monster level is 0 or unknown; mexp taken unchanged and "
                     "cannot be verified — 4.2 awards nothing at depth 0")
    return Value(max(1, round(zang_mexp / level)), "BAL-02", CONVERTED,
                 f"{zang_mexp} total / level {level}")


def armour_class(zang_ac: int | None) -> Value | None:
    """BAL-05 — rescale raw armour class by 9/8.

    2.8.1 and Zangband roll to-hit against ``ac * 3/4`` (cmd1.c:38);
    4.2 rolls against ``ac * 2/3`` (player-attack.c:207).  The scale-neutral
    factor is therefore (3/4) / (2/3) = 9/8, *not* the 3/2 that 4.2's
    armor-class comment suggests when read in isolation.
    """
    if zang_ac is None:
        return None
    return Value(round(zang_ac * 9 / 8), "BAL-05", CONVERTED,
                 f"{zang_ac} x 9/8 (to-hit divisor 3/4 -> 2/3)")


def hit_points(dice: tuple[int, int] | None) -> Value | None:
    """BAL-06 — convert ``NdM`` dice to 4.2's single average-hp integer."""
    if dice is None:
        return None
    count, sides = dice
    return Value(round(count * (sides + 1) / 2), "BAL-06", CONVERTED,
                 f"{count}d{sides} -> mean")


def sleepiness(zang_sleep: int | None, mapping: "SleepMapping | None") -> Value | None:
    """BAL-07 — map 2.8.1-era ``sleep`` onto 4.2's 0-255 ``sleepiness``.

    Zangband never changed ``sleep`` from 2.8.1 (median delta 0.0 across 450
    shared monsters), so the correct mapping is whichever one vanilla itself
    applied between 2.8.1 and 4.2.  :func:`derive_sleep_mapping` recovers it
    empirically from the monsters both versions share.
    """
    if zang_sleep is None:
        return None
    if mapping is None or not mapping.usable:
        return Value(zang_sleep, "BAL-07", INVENTED,
                     "no mapping recovered; value copied unchanged, which is "
                     "almost certainly wrong — 4.2 uses a 0-255 scale")
    return mapping.apply(zang_sleep)


# --- BAL-07 mapping recovery --------------------------------------------


@dataclass
class SleepMapping:
    """Empirical 2.8.1 -> 4.2 sleepiness relationship, recovered from shared monsters."""

    exact: dict[int, int]        # 2.8.1 value -> 4.2 value, where unambiguous
    ratio: float | None          # fallback multiplier
    sample: int
    ambiguous: int

    @property
    def usable(self) -> bool:
        return bool(self.exact) or self.ratio is not None

    def apply(self, value: int) -> Value:
        if value in self.exact:
            return Value(self.exact[value], "BAL-07", CONVERTED,
                         f"recovered mapping {value} -> {self.exact[value]}")
        if self.ratio is not None:
            return Value(min(255, round(value * self.ratio)), "BAL-07", DERIVED,
                         f"{value} x {self.ratio:.3f} (fitted; no exact mapping "
                         "observed for this value)")
        return Value(value, "BAL-07", INVENTED, "unmapped and no fallback ratio")


def derive_sleep_mapping(old_by_key: dict, new_by_key: dict) -> SleepMapping:
    """Recover 4.2's sleepiness scale from monsters shared with 2.8.1.

    Answers balance-calibration open question 1.  For each shared monster we
    observe (2.8.1 sleep, 4.2 sleepiness); where a 2.8.1 value maps consistently
    to one 4.2 value we record it exactly, otherwise we fall back to the median
    ratio.
    """
    observed: dict[int, list[int]] = {}

    for key, old in old_by_key.items():
        new = new_by_key.get(key)
        if new is None:
            continue
        old_sleep = getattr(old, "sleep", None)
        new_sleep = new.get_int("sleepiness")
        if old_sleep is None or new_sleep is None:
            continue
        observed.setdefault(old_sleep, []).append(new_sleep)

    exact: dict[int, int] = {}
    ambiguous = 0
    ratios: list[float] = []

    for old_value, new_values in observed.items():
        uniq = set(new_values)
        if len(uniq) == 1:
            exact[old_value] = new_values[0]
        else:
            ambiguous += 1
            exact[old_value] = round(statistics.median(new_values))
        if old_value > 0:
            ratios.extend(v / old_value for v in new_values)

    return SleepMapping(
        exact=dict(sorted(exact.items())),
        ratio=statistics.median(ratios) if ratios else None,
        sample=sum(len(v) for v in observed.values()),
        ambiguous=ambiguous,
    )


# --- lethality -----------------------------------------------------------


@dataclass
class Lethality:
    """BAL-13 / DEC-09 — the project's primary balance dial.

    Applied to 4.2's values rather than replacing them, preserving 4.2's
    relative tuning while adopting Zangband's absolute lethality.  BAL-14 puts
    these in ``constants.txt``; the tool reads them so its calibration cannot
    silently assume 1.0.
    """

    hp_scale: float = 0.73
    ac_scale: float = 0.50

    def describe(self) -> str:
        return f"hp x{self.hp_scale}, ac x{self.ac_scale}"


# --- BAL-09 curve calibration -------------------------------------------


@dataclass
class Curve:
    """Median 4.2 statistic by depth, used to place monsters 4.2 has never seen."""

    by_depth: dict[int, dict[str, float]]
    band: int = 5

    @classmethod
    def fit(cls, entries: list, band: int = 5) -> "Curve":
        buckets: dict[int, dict[str, list[int]]] = {}
        for entry in entries:
            depth = entry.get_int("depth")
            if depth is None:
                continue
            slot = (depth // band) * band
            bucket = buckets.setdefault(slot, {"hp": [], "ac": [], "exp": []})
            for field_name, key in (("hp", "hit-points"),
                                    ("ac", "armor-class"),
                                    ("exp", "experience")):
                value = entry.get_int(key)
                if value is not None:
                    bucket[field_name].append(value)

        by_depth = {
            slot: {k: statistics.median(v) for k, v in stats.items() if v}
            for slot, stats in buckets.items()
        }
        return cls(by_depth=by_depth, band=band)

    def at(self, depth: int, statistic: str) -> float | None:
        """Median value of `statistic` at `depth`, taking the nearest populated band."""
        if not self.by_depth:
            return None
        slot = (depth // self.band) * self.band
        candidates = sorted(self.by_depth, key=lambda s: (abs(s - slot), s))
        for candidate in candidates:
            if statistic in self.by_depth[candidate]:
                return self.by_depth[candidate][statistic]
        return None

    def calibrate(self, depth: int, statistic: str, zang_value: float | None,
                  zang_curve: "Curve | None") -> Value | None:
        """Place a Zangband-only monster on 4.2's curve, preserving its relative role.

        BAL-09: Zangband's own numbers set the monster's *offset* from the curve
        — tanky, glassy, fast — never its absolute value.
        """
        target = self.at(depth, statistic)
        if target is None:
            return None

        if zang_value is not None and zang_curve is not None:
            peer = zang_curve.at(depth, statistic)
            if peer:
                offset = zang_value / peer
                # Clamp: beyond 4x either way we are extrapolating, not calibrating.
                offset = max(0.25, min(4.0, offset))
                return Value(max(1, round(target * offset)), "BAL-09", DERIVED,
                             f"4.2 median {target:.0f} at depth {depth} "
                             f"x {offset:.2f} relative role in Zangband")

        return Value(max(1, round(target)), "BAL-09", INVENTED,
                     f"4.2 median {target:.0f} at depth {depth}; no relative "
                     "signal available from Zangband")
