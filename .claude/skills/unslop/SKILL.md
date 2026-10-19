---
name: unslop
description: |
  Un-slop the ZangbandTK manual: rewrite AI-sounding prose in docs/*.rst so it
  reads like a person wrote it, without changing a fact or breaking the build.
  Use when asked to un-slop, humanize, de-AI or tidy the manual, the site
  pages, or any new chapter before it lands. Built from Wikipedia's "Signs of
  AI writing" (October 2026) and measured against this repo's own pre-2022
  Angband text. For prose outside the manual, the general humanizer skill
  also works.
---

# Un-slop the manual

The manual's 2026 pages were written with Claude, and they read like it:
"X, not Y" contrasts, dashes, bold sentence leads, words like *honest* and
*on purpose*, notes about milestones, and no contractions at all. The
chapters inherited from Angband were written by people before November 2022,
and they read like people: "You'll probably die rather quickly", "That's
about it - everything else is optional." The job is to make the new text
sound like the old, without losing anything it says.

The rules are in [naughty-list.md](naughty-list.md). Read it before the first
edit of a session. The two scripts measure the work:

- `scripts/slop_lint.py` counts Naughty List patterns in prose, skipping code,
  literals and markup. The `--new-only` switch looks only at lines committed
  after ChatGPT's launch (30 November 2022), by `git blame`. `--old-only` looks
  only at the older lines. `-v` lists every hit with its line.
- `scripts/fact_check.py` compares a file with `HEAD` and lists every number,
  literal, role, link, quoted string, heading, label and directive line that
  changed. A rewrite that only changes wording lists nothing.

Run both with `python3 -I` from the repository root.

## Scope

Work only on lines this project wrote after November 2022, and leave the rest
alone. The inherited Angband text is the model voice here. It isn't the
problem, and touching it loses history. That includes Angband's own 2023 to
2025 commits, by backwardsEric, Eric Branlund and others, which came in with
the 4.2.x base: those were written by people too. `slop_lint.py --ranges FILE`
prints the line ranges that count as this project's (by `git blame` author
and date), and `-v --new-only` lists the hits inside them. If a sentence you're
rewriting runs across an old line, you may edit the old line as part of that
sentence, and nothing more.

Don't run this on:

- `docs/diary.rst`. It's a first-person journal with its own rule: entries
  are never quietly rewritten (see the diary memory). Do it only if Steven asks
  for that specific entry.
- `docs/releases.rst`, unless asked. It's a log, and logs are allowed to
  sound like logs.
- `docs/version.rst`, `docs/copying.rst`, `docs/thanks.rst`. They're Angband's
  and Zangband's history and licence text, and `meta.rst` says they're kept
  as they were.
- Anything inside code blocks, literals, tables' structure, or data files.

## The voice

A player who knows the game, writing for other players. Concretely:

- Talk to the reader as *you*, and say what happens in the game first. Give
  the reason after, if the player can use it.
- Plain words: *is*, *has*, *uses*, *gets*, *lets you*. Say the number without
  bolding it.
- Contractions where you'd say them aloud (*don't*, *it's*, *you'll*,
  *there's*). Reference tables and option lists can stay formal, as Angband's
  did.
- Mix short sentences with long ones. Commit to claims: "this is the best
  early weapon" beats a hedge, and *usually*, *very* and *perhaps* are fine.
- Humour only where it's already there, or where the game is funny on its
  own. Don't add a joke per section; forced jokes are another tell.
- British spelling, as the manual already uses: *armour*, *colour*,
  *behaviour*, *levelling*. Code identifiers keep their own spelling
  (`lethality:armor-class`).
- Steven's own writing is plain and direct ("Here you find a redesign of the
  flags window. This design also adds a lot of tooltip help which is very
  helpful for the player."). Aim for that register, edited: no typos, no
  flourishes.

## How to work, per file

1. **Measure.** `python3 -I .claude/skills/unslop/scripts/slop_lint.py
   --new-only -v docs/FILE.rst`. Note the score.
2. **Read the whole file** before changing anything. Several tells are only
   visible at paragraph scale: the same contrast on every page, a closer
   after every section, the same triad on three pages.
3. **Rewrite paragraph by paragraph**, not phrase by phrase. For each one ask
   what the player learns from it, then write that. Delete sentences that
   only stage, defend or grade other sentences (N1 to N4, N8, N31). Don't
   rephrase them: deleting is the fix. Patching flagged words one at a time
   leaves the skeleton in place.
4. **Keep, exactly:** every number, name, key, command, file path, option
   name, monster, item and spell name, quoted game message, and every claim.
   Keep every odd, specific detail; those are what make text read as human.
   Add no facts. If a vague sentence needs a detail you don't have, write a
   simpler sentence or leave it.
5. **Keep the RST intact:** directives and their options, roles
   (`:doc:`...``), inline literals, link targets, labels, substitutions,
   table structure, indentation, and heading text and underline length.
   Don't change heading text (anchors and `` `Title`_ `` links depend on it).
6. **Check.**
   - `slop_lint.py --new-only` again. The inherited human text scores about
     7 per 1000 words. Aim for under 12, and don't push for 0: some hits are
     correct uses, and a page with none of them reads as if it was scrubbed.
   - `fact_check.py docs/FILE.rst`. Every `CHECK` line must be explained:
     a fact you moved but kept, a duplicate you removed, or something you put
     back. A `heading` line means you broke rule 5.
   - `git diff --word-diff docs/FILE.rst`. Read it as a stranger would.
7. **When a batch is done, build** the site the way the CI gate does, outside
   the tree:

   ```sh
   .venv-docs/bin/sphinx-build -M html docs "$SCRATCH/build" -W --keep-going -q
   ```

   It must finish with no warnings.

## Things you'll notice and must not fix silently

A rewrite reads every sentence closely, so it turns up stale facts: status
lines a few milestones behind, a count that no longer matches the data, a
feature described as planned that shipped. Don't fix those as part of a
style pass. List them in the report with the line, so Steven can decide.
The website memory says which lines go stale.

## Report

Per file: the score before and after, the `fact_check.py` lines with a
reason for each, and anything from the section above. Keep it short.
