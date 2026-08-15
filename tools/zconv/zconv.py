#!/usr/bin/env python3
"""
zconv — convert Zangband 2.7.5 game data onto Angband 4.2's data model.

Implements BAL-01 through BAL-14 from
``.claude/plans/phase1-balance-calibration.md``.  The data files this produces
are a side effect; the review report is the deliverable (BAL-11).

Usage::

    ./zconv.py analyse            # derive scales and report, write nothing
    ./zconv.py monsters           # convert r_info.txt -> monster.txt fragment
    ./zconv.py monsters --write   # ...and write the data file

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

import argparse
import re
import sys
import tomllib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import aformat  # noqa: E402
import rules  # noqa: E402
import zformat  # noqa: E402
from report import Converted, Report  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]

ZANGBAND = ROOT / "archive" / "zangband" / "lib" / "edit"
ANGBAND281 = ROOT / "archive" / "angband-281" / "lib" / "edit"
GAMEDATA = ROOT / "lib" / "gamedata"
OUTDIR = Path(__file__).resolve().parent / "out"


def load_valid_flags() -> set[str]:
    """4.2's monster race and spell flags, read from the source of truth."""
    flags: set[str] = set()
    for name, pattern in (
        ("list-mon-race-flags.h", r"^RF\(([A-Z0-9_]+)"),
        ("list-mon-spells.h", r"^RSF\(([A-Z0-9_]+)"),
    ):
        path = ROOT / "src" / name
        if not path.exists():
            continue
        rx = re.compile(pattern, re.M)
        flags.update(rx.findall(path.read_text(encoding="utf-8", errors="replace")))
    flags.discard("NONE")
    return flags


def load_bases() -> dict[str, str]:
    """Map display glyph to 4.2 monster_base template name.

    Every 4.2 monster carries ``base:``, which supplies default flags and pain
    messages, so imported entries need one too.  monster_base.txt keys templates
    by glyph and the mapping is 1:1 apart from ``P``, where the generic 'giant'
    is preferred over the Morgoth-specific template.
    """
    path = GAMEDATA / "monster_base.txt"
    if not path.exists():
        return {}

    bases: dict[str, str] = {}
    name: str | None = None
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if line.startswith("name:"):
            name = line[5:].strip()
        elif line.startswith("glyph:") and name:
            glyph = line[6:].strip()
            if glyph == "P":
                bases.setdefault(glyph, "giant")
            else:
                bases.setdefault(glyph, name)
    return bases


def load_overrides() -> dict:
    """Hand-tuned values, reapplied on every run (BAL-12)."""
    path = Path(__file__).resolve().parent / "overrides.toml"
    if not path.exists():
        return {}
    with path.open("rb") as handle:
        return tomllib.load(handle)


def load_lethality() -> rules.Lethality:
    """BAL-14 — read the scalars from constants.txt if present, else the BAL-13 defaults."""
    path = GAMEDATA / "constants.txt"
    scalars = {}
    if path.exists():
        text = path.read_text(encoding="utf-8", errors="replace")
        for key, field in (("lethality:hit-points", "hp_scale"),
                           ("lethality:armor-class", "ac_scale")):
            m = re.search(rf"^{re.escape(key)}:([0-9.]+)$", text, re.M)
            if m:
                scalars[field] = float(m.group(1))
    return rules.Lethality(**scalars)


THEME = re.compile(
    r"amber|chaos|cthulhu|nyarlathotep|hastur|shub|azathoth|yog|tsathoggua|"
    r"benedict|corwin|eric|julian|caine|gerard|fiona|bleys|brand|random|"
    r"pattern|trump|unicorn|logrus",
    re.I,
)


def is_themed(mon: zformat.Monster) -> bool:
    """DEC-19 'theme first': does this monster carry Amber, Mythos or Chaos identity?"""
    return bool(THEME.search(mon.name) or THEME.search(mon.desc))


def cmd_analyse(args) -> int:
    """Derive scales from the corpus and report, without writing anything."""
    old = zformat.read_monsters(str(ANGBAND281 / "r_info.txt"))
    zang = zformat.read_monsters(str(ZANGBAND / "r_info.txt"))
    new = aformat.parse(str(GAMEDATA / "monster.txt"))

    old_by_key = {m.key: m for m in old}
    zang_by_key = {m.key: m for m in zang}
    new_by_key = aformat.index_by_key(new)

    print(f"corpus: 2.8.1={len(old)}  zangband={len(zang)}  4.2={len(new)}")
    print(f"shared 2.8.1<->4.2: {len(set(old_by_key) & set(new_by_key))}")
    print(f"zangband-only vs both: "
          f"{len([k for k in zang_by_key if k not in old_by_key and k not in new_by_key])}")
    print()

    mapping = rules.derive_sleep_mapping(old_by_key, new_by_key)
    print("BAL-07 sleepiness mapping (balance-calibration open question 1)")
    print(f"  observations:      {mapping.sample}")
    print(f"  distinct 2.8.1 values mapped: {len(mapping.exact)}")
    print(f"  ambiguous (one 2.8.1 value -> several 4.2 values): {mapping.ambiguous}")
    print(f"  fallback ratio:    "
          f"{mapping.ratio:.4f}" if mapping.ratio else "  fallback ratio:    none")
    print()
    print("  2.8.1 -> 4.2 (first 20):")
    for old_value, new_value in list(mapping.exact.items())[:20]:
        ratio = f"x{new_value / old_value:.2f}" if old_value else "n/a"
        print(f"    {old_value:>4} -> {new_value:>4}   {ratio}")

    lethality = load_lethality()
    print()
    print(f"BAL-13 lethality scalar: {lethality.describe()}")

    curve = rules.Curve.fit(new)
    print(f"BAL-09 4.2 curve fitted over {len(curve.by_depth)} depth bands")
    return 0


def cmd_monsters(args) -> int:
    """Convert Zangband-only monsters onto 4.2's monster.txt model."""
    old = zformat.read_monsters(str(ANGBAND281 / "r_info.txt"))
    zang = zformat.read_monsters(str(ZANGBAND / "r_info.txt"))
    new = aformat.parse(str(GAMEDATA / "monster.txt"))

    old_by_key = {m.key: m for m in old}
    new_by_key = aformat.index_by_key(new)

    valid_flags = load_valid_flags()
    bases = load_bases()
    overrides = load_overrides().get("monster", {})
    lethality = load_lethality()
    mapping = rules.derive_sleep_mapping(old_by_key, new_by_key)
    curve_42 = rules.Curve.fit(new)
    curve_z = _zangband_curve(zang)

    candidates = [m for m in zang
                  if m.key not in old_by_key and m.key not in new_by_key]
    if args.theme_only:
        themed = [m for m in candidates if is_themed(m)]
    else:
        themed = candidates

    report = Report(
        title="zconv — monster conversion review",
        source=str(ZANGBAND / "r_info.txt"),
        target=str(GAMEDATA / "monster.txt"),
        lethality=lethality.describe(),
    )
    report.notes.append(
        f"{len(candidates)} monsters exist in Zangband but in neither Angband 2.8.1 "
        f"nor 4.2.6."
    )
    if args.theme_only:
        report.notes.append(
            f"DEC-19 'theme first' filter applied: {len(themed)} of {len(candidates)} "
            f"carry Amber, Mythos or Chaos identity. The remaining "
            f"{len(candidates) - len(themed)} were not converted — rerun without "
            "--theme-only to include them."
        )
    report.notes.append(
        "BAL-13's lethality scalar is applied by the game at load time, not baked "
        "into these values, so the numbers here are pre-scalar and comparable with "
        "4.2's own data."
    )
    if mapping.ambiguous:
        report.notes.append(
            f"BAL-07: {mapping.ambiguous} source sleep values map to more than one "
            "4.2 sleepiness value; the median was taken for those."
        )

    entries: list[aformat.Entry] = []

    for mon in sorted(themed, key=lambda m: m.index):
        item = Converted(name=mon.name, source_index=mon.index)
        entry = aformat.Entry()
        entry.set("name", zformat.normalise_name(mon.name).lower())

        depth = mon.level if mon.level is not None else 0

        def record(key: str, value: rules.Value | None) -> None:
            if value is None:
                return
            item.fields[key] = value
            entry.set(key, value.value)

        # 'base' is mandatory in 4.2 — all 624 of its own monsters carry one.
        if mon.symbol and mon.symbol in bases:
            record("base", rules.Value(bases[mon.symbol], "CNT-01", rules.CONVERTED,
                                       f"inferred from glyph '{mon.symbol}'"))
        elif mon.symbol:
            record("base", rules.Value("person", "CNT-01", rules.INVENTED,
                                       f"glyph '{mon.symbol}' has no 4.2 monster_base "
                                       "template; placeholder base assigned"))
        else:
            record("base", rules.Value("person", "CNT-01", rules.INVENTED,
                                       "no glyph in source; placeholder base assigned"))

        if mon.colour:
            item.fields["color"] = rules.Value(mon.colour, "CNT-01", rules.EXACT)
            entry.set("color", mon.colour)
        if mon.symbol:
            item.fields["glyph"] = rules.Value(mon.symbol, "CNT-01", rules.EXACT)
            entry.set("glyph", mon.symbol)

        if mon.speed is not None:
            record("speed", rules.Value(mon.speed, "CNT-01", rules.EXACT,
                                        "Zangband never changed speed from 2.8.1"))

        hp = rules.hit_points(mon.hp_dice)
        if hp is None:
            hp = curve_42.calibrate(depth, "hp", None, None)
        record("hit-points", hp)

        record("armor-class", rules.armour_class(mon.ac))
        record("sleepiness", rules.sleepiness(mon.sleep, mapping))
        if mon.vision is not None:
            record("hearing", rules.Value(mon.vision, "CNT-01", rules.EXACT))
        record("depth", rules.Value(depth, "CNT-01", rules.EXACT))
        if mon.rarity is not None:
            record("rarity", rules.Value(mon.rarity, "CNT-01", rules.EXACT))

        exp = rules.experience(mon.mexp, mon.level)
        if exp is None:
            exp = curve_42.calibrate(depth, "exp",
                                     float(mon.mexp) if mon.mexp else None, curve_z)
        record("experience", exp)

        for blow in mon.blows:
            entry.pairs.append(("blow", blow))

        mapped = [f for f in mon.flags if f in valid_flags]
        item.unmapped_flags = [f for f in mon.flags if f not in valid_flags]
        if mapped:
            entry.set("flags", " | ".join(mapped))

        if mon.desc:
            entry.set("desc", mon.desc)

        # BAL-12 — overrides reapplied last so a re-run never discards tuning.
        for key, value in overrides.get(mon.key, {}).items():
            entry.set(key, value)
            item.fields[key] = rules.Value(value, "BAL-12", rules.EXACT,
                                           "hand-tuned in overrides.toml")
            item.overridden.append(key)

        report.items.append(item)
        entries.append(entry)

    OUTDIR.mkdir(exist_ok=True)
    report_path = OUTDIR / "monsters.report.md"
    report_path.write_text(report.render(), encoding="utf-8")
    print(f"report:  {report_path.relative_to(ROOT)}")

    if args.write:
        data_path = OUTDIR / "monster.zangband.txt"
        aformat.write(
            str(data_path), entries,
            preamble=(
                "# monster.zangband.txt — generated by tools/zconv. Do not hand-edit.\n"
                "# Hand-tuned values belong in tools/zconv/overrides.toml (BAL-12).\n"
                f"# Source: {ZANGBAND / 'r_info.txt'}\n"
            ),
        )
        print(f"data:    {data_path.relative_to(ROOT)}")
    else:
        print("data:    not written (pass --write)")

    invented = sum(len(i.invented) for i in report.items)
    print(f"entries: {len(report.items)}   invented values: {invented}   "
          f"unmapped flags: {len({f for i in report.items for f in i.unmapped_flags})}")
    return 0


def _zangband_curve(monsters: list[zformat.Monster]) -> rules.Curve:
    """Fit Zangband's own depth curve, so BAL-09 can read a monster's relative role."""
    fake: list[aformat.Entry] = []
    for mon in monsters:
        if mon.level is None:
            continue
        entry = aformat.Entry()
        entry.set("name", mon.name)
        entry.set("depth", mon.level)
        if mon.hp_dice:
            count, sides = mon.hp_dice
            entry.set("hit-points", round(count * (sides + 1) / 2))
        if mon.ac is not None:
            entry.set("armor-class", mon.ac)
        if mon.mexp is not None and mon.level:
            entry.set("experience", max(1, round(mon.mexp / mon.level)))
        fake.append(entry)
    return rules.Curve.fit(fake)


def main() -> int:
    parser = argparse.ArgumentParser(
        prog="zconv", description="Convert Zangband data onto Angband 4.2's model.")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("analyse", help="derive scales and report; write nothing")

    monsters = sub.add_parser("monsters", help="convert r_info.txt to 4.2 monster entries")
    monsters.add_argument("--write", action="store_true",
                          help="write the data file as well as the report")
    monsters.add_argument("--theme-only", action="store_true",
                          help="DEC-19: only monsters with Amber/Mythos/Chaos identity")

    args = parser.parse_args()
    return {"analyse": cmd_analyse, "monsters": cmd_monsters}[args.command](args)


if __name__ == "__main__":
    raise SystemExit(main())
