#!/usr/bin/env python3
"""Count Naughty List patterns in reStructuredText prose.

Usage:
    python3 -I slop_lint.py [options] FILE_OR_DIR ...

    -v, --verbose   list every hit with its line number
    --new-only      only look at lines this project wrote after ChatGPT's
                    launch (30 Nov 2022), by git blame.  Older lines, and
                    newer ones by upstream Angband developers, are human
    --old-only      only the other lines: the human baseline
    --ranges        print the line ranges --new-only would look at, and stop
    --authors RE    who counts as this project (default: the git user and
                    z88kat); everyone else is upstream
    --rule N1,N18   only these rules

Code, literals, directives, link targets and table rules are skipped, so
a hit is always in prose a reader sees.  The numbers are for comparing a
page before and after an edit, not a verdict: every rule has innocent uses,
and the Naughty List says which ones need company before they count.
"""

import argparse
import bisect
import math
import os
import re
import subprocess
import sys

CHATGPT_LAUNCH = 1669766400  # 2022-11-30 00:00 UTC

# id, short name, weight (2 strong, 1 medium, 0.5 weak), regexes.
# Regexes run case-insensitively on prose with inline markup stripped.
RULES = [
    ("N1", "not X but Y / X, not Y / rather than", 2, [
        r"\bnot (?:just|only|merely|simply)\b",
        r"\b(?:is|are|was|were|it's|that's) not\b[^.;:?!]{1,60}?[,;] (?:but|it's|it is|they're|they are)\b",
        r"\bnot\b[^.;:?!]{1,50}?, but\b",
        r"\brather than\b",
        r"\w, not (?:a|an|the|in|on|by|for|to|as|because)?\s?\w+",
        r"\bno \w+, no \w+",
    ]),
    ("N2", "denial nobody asked for", 2, [
        r"(?:^|[.:;] )(?:it|this|that|these|those|which|they) (?:is|are|was|were)(?: not|n't) (?:a|an|the|just|merely|simply|only)\b",
        r"\b(?:is|are|was|were)(?: not|n't) (?:invented|made up|guesswork|a guess|an accident|arbitrary|a bug|a gap|an oversight)\b",
        r"\b(?:is|are)(?: not|n't) [\w']+ alone\b",
        r"\bnone of (?:it|this|that) is\b",
    ]),
    ("N3", "one-line closer or aphorism", 2, []),  # paragraph-level, below
    ("N4", "staged run-up or page describing itself", 2, [
        r"\bthis (?:page|chapter|section|guide|note|table|list) (?:is|describes|covers|states|explains|lists|sets out|gives)\b",
        r"\b(?:two|three|four|five) things\b",
        r"\bin the order you\b",
        r"\bworth (?:stating|saying|noting|knowing|explaining|spelling out|being clear)\b",
        r"\bhere(?:'s| is) (?:what|how|why|the)\b",
        r"\b(?:follows?|follow) immediately\b",
        r"\blet(?:'s| us) (?:look|start|begin|dive|be clear)\b",
    ]),
    ("N5", "rule of three", 0.5, [
        r"\b\w+(?: \w+)?, \w+(?: \w+)?,? and \w+(?: \w+)?\b[.;:]",
    ]),
    ("N6", "summary closer", 1, [
        r"\b(?:in short|in summary|in conclusion|to sum up|all told|that is the whole of)\b",
        r"^overall,",
    ]),
    ("N7", "puffed-up significance", 2, [
        r"\b(?:testament|pivotal|indelible|deeply rooted|evolving landscape|focal point)\b",
        r"\b(?:crucial|vital|key|central) (?:role|part|moment)\b",
        r"\bunderscor\w*",
        r"\bmarks? an? (?:shift|turning point|milestone)\b",
        r"\bsetting the stage\b",
        r"\breflects? (?:a )?broader\b",
        r"\blegacy\b",
    ]),
    ("N8", "sincerity and intent flags", 1, [
        r"\bhonest(?:ly)?\b", r"\bplainly\b", r"\bsimply\b", r"\bgenuine(?:ly)?\b",
        r"\bactually\b", r"\bin fact\b", r"\bon purpose\b", r"\bdeliberate(?:ly)?\b",
        r"\bby design\b", r"\bsays so\b", r"\bworth\b", r"\bthe point\b",
        r"\bquietly\b", r"\bsilently\b", r"\ba real \w+", r"\bthe whole of\b",
        r"\bwhichever direction\b", r"\bfor what it is\b",
    ]),
    ("N9", "brochure language", 2, [
        r"\b(?:boasts?|vibrant|profound|showcas\w+|nestled|groundbreaking|renowned|"
        r"diverse array|seamless(?:ly)?|stunning|breathtaking|world-class|"
        r"cutting-edge|exciting|immersive|unparalleled|rich (?:history|tapestry|heritage|world))\b",
        r"\bin the heart of\b",
    ]),
    ("N10", "-ing rider on a fact", 1, [
        r", (?:highlighting|underscoring|emphasi[sz]ing|ensuring|reflecting|symboli[sz]ing|"
        r"contributing to|cultivating|fostering|encompassing|enhancing|showcasing|"
        r"cementing|solidifying|reinforcing|creating a)\b",
    ]),
    ("N11", "vague connection or attribution", 1, [
        r"\b(?:associated with|in connection with|connected (?:to|with)|linked to|tied to)\b",
        r"\b(?:many|most|some) (?:players|people|veterans|fans)\b",
        r"\bexperts?\b",
        r"\bit is (?:often|widely|generally|commonly) (?:said|thought|believed|held)\b",
    ]),
    ("N12", "status brag or challenges-and-outlook", 1, [
        r"\bdespite (?:these|its|the) challenges\b", r"\bfaces? (?:several |many )?challenges\b",
        r"\bfuture (?:outlook|prospects)\b", r"\blooks bright\b",
        r"\bworth standing behind\b", r"\bfeature-complete\b",
    ]),
    ("N13", "AI vocabulary", 1, [
        r"^additionally\b", r"\balign(?:s|ed)? with\b", r"\bbolster\w*", r"\bcrucial\b",
        r"\bdeep dive\b", r"\bdelv\w+", r"\bemphasi[sz]\w*", r"\benduring\b", r"\benhanc\w*",
        r"\bfoster\w*", r"\bgarner\w*", r"\bhighlight\w*", r"\binterplay\b", r"\bintricac?\w*",
        r"\blandscape\b", r"\bmeticulous\w*", r"\brobust\b", r"\btapestry\b", r"\bvaluable\b",
        r"\bclearer\b", r"\bdependabl\w+", r"\bechoe[sd]\b", r"\bleverag\w*", r"\bmatters\b",
        r"\bmultifaceted\b", r"\bprioriti[sz]\w*", r"\bsteady\b", r"\buniversally\b",
        r"\bcausal\b", r"\bempirical\b", r"\bnuanced?\b", r"\bnotably\b",
    ]),
    ("N14", "avoiding is/has", 1, [
        r"\b(?:serves?|stands?|functions?|acts?|operates?) as\b",
        r"\b(?:boasts|offers|maintains|represents|refers to)\b",
    ]),
    ("N15", "stiff synonym", 0.5, [
        r"\butili[sz]\w+", r"\bcommenc\w+", r"\bfacilitat\w+", r"\brelocat\w+",
        r"\bendeavou?r\w*", r"\bprior to\b", r"\bin order for\b", r"\bpossess(?:es)?\b",
    ]),
    ("N18", "dash", 1, [
        "—", r" – ", r"(?<=\w) -- (?=\w)", r"(?<=\w)---(?=\w)",
    ]),
    ("N21", "decorative bold", 1, []),        # computed on raw text, below
    ("N22", "inline-header list item", 2, []),  # computed on raw text, below
    ("N24", "emoji or decorative symbol", 2, [
        "[✅✔✓✨⚠\U0001F300-\U0001FAFF]",
    ]),
    ("N25", "Markdown in RST", 2, []),        # computed on raw text, below
    ("N26", "curly quote in source", 0.5, ["[“”‘’]"]),
    ("N27", "chat residue", 2, [
        r"\b(?:i hope this helps|great question|let me know|would you like|feel free to|"
        r"happy (?:gaming|adventuring|hunting|delving))\b",
        r"^(?:certainly|of course|sure)!",
    ]),
    ("N28", "hedging about sources or use", 2, [
        r"\bshould be (?:treated|read|taken|understood|seen) as\b",
        r"\bdoes not by itself\b", r"\bbased on (?:the )?available\b",
        r"\bnot (?:widely |publicly |well )?documented\b",
    ]),
    ("N29", "didactic disclaimer", 1, [
        r"\b(?:it(?:'s| is)) (?:important|worth|crucial|vital) to (?:note|remember|know|keep in mind)\b",
        r"\bworth noting\b", r"\bkeep in mind\b", r"\bbear in mind\b", r"\bmay vary\b",
    ]),
    ("N30", "placeholder", 2, [
        r"\b(?:TODO|TBD|FIXME)\b", r"\[insert", r"\(if available\)", r"\b20\d\d-xx",
    ]),
    ("N31", "process residue (edit-summary voice)", 1, [
        r"\b(?:has|have) been (?:checked|corrected|updated|revised|adapted|touched|rewritten|reviewed|swept)\b",
        r"\b(?:preserv|retain)(?:e|ed|es|ing)\b",
        r"\bkept as (?:it|they) (?:was|were)\b", r"\bnothing else (?:has been|was) \w+",
        r"\bmilestone\b", r"\b(?:PLR|DEC|WLD|OBJ|MON|TWN|DNG|QST|REQ)-\d+",
        r"\bM\d{1,2}\b", r"\bas it stands\b",
    ]),
]

# Contractions people use in a manual like this one, and the spelled-out forms
# a model prefers.  Used for the register line, not for a rule score.
CONTRACTED = re.compile(
    r"\b(?:\w+n't|it's|that's|there's|here's|what's|you're|you'll|you've|you'd|"
    r"we're|we'll|they're|they'll|I'm|I've|I'll|let's|who's|isn't)\b", re.I)
SPELLED = re.compile(
    r"\b(?:does not|do not|did not|is not|are not|was not|were not|cannot|can not|"
    r"will not|would not|could not|should not|has not|have not|had not|"
    r"it is|that is|there is|you are|you will|you have|they are|we are)\b", re.I)

CODE_DIRECTIVES = {"code-block", "code", "sourcecode", "literalinclude", "parsed-literal",
                   "math", "raw", "highlight", "csv-table", "graphviz", "image",
                   "figure", "toctree", "include", "only"}
ADORNMENT = re.compile(r"^([=\-~^\"'`#*+<>:._])\1{2,}\s*$")
TABLE_RULE = re.compile(r"^\s*(?:[+|][-=+|]+[+|]|=+(?:\s+=+)+)\s*$")
DIRECTIVE = re.compile(r"^(\s*)\.\.\s+([\w:-]+)::")
FIELD = re.compile(r"^\s*:[\w .-]+:(?:\s|$)")


def strip_inline(text):
    """Remove inline markup a reader never sees as prose."""
    text = re.sub(r"``.+?``", " LITERAL ", text)
    text = re.sub(r":[\w:]+:`([^`<]*?)\s*<[^>]*>`", r"\1", text)
    text = re.sub(r":[\w:]+:`([^`]*)`", r"\1", text)
    text = re.sub(r"`([^`<]*?)\s*<[^>]*>`__?", r"\1", text)
    text = re.sub(r"`([^`]*)`__?", r"\1", text)
    text = re.sub(r"https?://\S+", " URL ", text)
    text = re.sub(r"\|[\w -]+\|", " SUB ", text)
    return text


def prose_lines(lines):
    """Yield (lineno, raw, prose) for lines a reader sees as running text."""
    literal_indent = None  # inside a literal or code block deeper than this
    for i, raw in enumerate(lines, 1):
        line = raw.rstrip("\n")
        stripped = line.strip()
        indent = len(line) - len(line.lstrip())
        if literal_indent is not None:
            if stripped == "" or indent > literal_indent:
                continue
            literal_indent = None
        if stripped == "":
            continue
        m = DIRECTIVE.match(line)
        if m:
            name = m.group(2).split(":")[-1]
            if name in CODE_DIRECTIVES:
                literal_indent = indent
                continue
            rest = line[m.end():].strip()   # e.g. the card title of grid-item-card
            if rest:
                yield i, line, strip_inline(rest)
            continue
        if stripped.startswith(".."):
            literal_indent = indent  # comment or target, and anything under it
            continue
        if ADORNMENT.match(stripped) or TABLE_RULE.match(line) or FIELD.match(line):
            continue
        if stripped.endswith("::"):
            literal_indent = indent
            stripped = stripped[:-1]
        text = re.sub(r"^\s*(?:[-*+]|\d+[.)]|#\.)\s+", "", stripped)  # list markers
        text = re.sub(r"^\*\s+-\s+", "", text)                        # list-table cells
        text = text.strip("| ")
        if text in ("", "-", "—", "–"):
            continue
        yield i, line, strip_inline(text)


def project_authors(override):
    if override:
        return re.compile(override)
    try:
        me = subprocess.run(["git", "config", "user.name"], capture_output=True,
                            text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        me = ""
    # Uncommitted lines blame as "Not Committed Yet": they're the ones being edited.
    return re.compile("|".join(re.escape(n) for n in (me, "z88kat", "Not Committed Yet") if n))


def new_lines(path, authors):
    """Line numbers this project wrote after ChatGPT's launch, and the line count."""
    blame = blame_times(path)
    new = {n for n, (t, a) in blame.items() if t > CHATGPT_LAUNCH and authors.search(a)}
    return new, len(blame)


def blame_times(path):
    """Map line number -> (commit time, author), or {} when git blame fails."""
    try:
        out = subprocess.run(["git", "blame", "--line-porcelain", "--", os.path.abspath(path)],
                             capture_output=True, text=True, check=True,
                             cwd=os.path.dirname(os.path.abspath(path)) or ".").stdout
    except (OSError, subprocess.CalledProcessError):
        return {}
    times, lineno, t, author = {}, 0, 0, ""
    for row in out.splitlines():
        if row.startswith("author "):
            author = row[7:]
        elif row.startswith("committer-time "):
            t = int(row.split()[1])
        elif row.startswith("\t"):
            lineno += 1
            times[lineno] = (t, author)
    return times


class Para:
    """Consecutive prose rows joined, so a pattern can cross a line break."""

    def __init__(self, rows):
        self.rows = rows
        self.starts, self.raw_starts = [], []
        prose, raw, p, q = [], [], 0, 0
        for lineno, r, t in rows:
            self.starts.append(p)
            self.raw_starts.append(q)
            prose.append(t)
            raw.append(r.strip())
            p += len(t) + 1
            q += len(r.strip()) + 1
        self.text = " ".join(prose)
        self.raw = " ".join(raw)

    def line_at(self, offset, raw=False):
        starts = self.raw_starts if raw else self.starts
        i = max(0, bisect.bisect_right(starts, offset) - 1)
        return self.rows[i][0]


def paragraphs(rows):
    """Group consecutive prose rows into paragraphs; a list item starts a new one."""
    paras, cur, last = [], [], None
    for row in rows:
        lineno, raw = row[0], row[1]
        is_item = bool(re.match(r"^\s*(?:[-*+]|\d+[.)]|#\.)\s", raw))
        if cur and (lineno != last + 1 or is_item):
            paras.append(Para(cur))
            cur = []
        cur.append(row)
        last = lineno
    if cur:
        paras.append(Para(cur))
    return paras


SENTENCE_END = re.compile(r"(?<=[.!?])[\"')\]]*\s+(?=[A-Z\"'(*])")


def sentences(text):
    return [s for s in SENTENCE_END.split(text) if len(s.split()) >= 2]


def lint(path, args):
    with open(path, encoding="utf-8") as f:
        lines = f.readlines()
    rows = list(prose_lines(lines))
    new_share = None
    if args.new_only or args.old_only:
        new, total = new_lines(path, args.authors_re)
        if total:
            new_share = len(new) / max(1, total)
            keep = new if args.new_only else set(range(1, total + 1)) - new
            rows = [r for r in rows if r[0] in keep]
    wanted = set(args.rule.split(",")) if args.rule else None
    hits = {rid: [] for rid, *_ in RULES}
    compiled = {rid: [re.compile(p, re.I | re.M) for p in pats] for rid, _, _, pats in RULES}

    paras = paragraphs(rows)
    lens = []
    for para in paras:
        text = para.text
        for rid, regs in compiled.items():
            for rx in regs:
                for m in rx.finditer(text):
                    hits[rid].append((para.line_at(m.start()),
                                      text[max(0, m.start() - 30):m.end() + 30]))
        first_raw = para.rows[0][1]
        if not re.match(r"^\s*\*\s+-\s+\*\*[^*]+\*\*\s*$", first_raw):  # table row labels
            for m in re.finditer(r"\*\*[^*]+\*\*", para.raw):
                hits["N21"].append((para.line_at(m.start(), raw=True), m.group(0)))
        if re.match(r"^\s*(?:[-*+]|\d+[.)])\s+\*\*[^*]+\*\*\s*[:—–-]\s*\S", para.raw) or \
           re.match(r"^\s*(?:[-*+]|\d+[.)])\s+\*\*[^*]+[.:]\*\*\s*\S", para.raw):
            hits["N22"].append((para.rows[0][0], first_raw.strip()[:70]))
        for lineno, raw, _ in para.rows:
            if re.search(r"\[[^\]]+\]\((?:https?://|/|\.)[^)]*\)|^\s*```|^#{1,6} \w", raw):
                hits["N25"].append((lineno, raw.strip()[:70]))
        sents = sentences(text)
        lens.extend(len(s.split()) for s in sents)
        words = len(text.split())
        is_item = bool(re.match(r"^\s*(?:[-*+]|\d+[.)]|#\.)\s", first_raw))
        if not is_item and len(sents) == 1 and 2 <= words <= 9 \
                and text.rstrip().endswith((".", "!")):
            hits["N3"].append((para.rows[0][0], text[:70]))

    words = sum(len(p.split()) for _, _, p in rows)
    contracted = sum(len(CONTRACTED.findall(p)) for _, _, p in rows)
    spelled = sum(len(SPELLED.findall(p)) for _, _, p in rows)
    if wanted:
        hits = {k: v for k, v in hits.items() if k in wanted}
    weights = {rid: w for rid, _, w, _ in RULES}
    score = sum(weights[k] * len(v) for k, v in hits.items())
    return {
        "path": path, "words": words, "hits": hits, "new_share": new_share,
        "score": 1000.0 * score / max(1, words),
        "lens": lens, "contracted": contracted, "spelled": spelled,
    }


def rhythm(lens):
    if not lens:
        return "no sentences"
    mean = sum(lens) / len(lens)
    sd = math.sqrt(sum((x - mean) ** 2 for x in lens) / len(lens))
    band = 100.0 * sum(1 for x in lens if 8 <= x <= 20) / len(lens)
    return "%d sentences, mean %.1f words, sd %.1f, %.0f%% in the 8-20 band" % (
        len(lens), mean, sd, band)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("paths", nargs="+")
    ap.add_argument("-v", "--verbose", action="store_true")
    ap.add_argument("--new-only", action="store_true")
    ap.add_argument("--old-only", action="store_true")
    ap.add_argument("--ranges", action="store_true")
    ap.add_argument("--rule")
    ap.add_argument("--authors")
    args = ap.parse_args()
    args.authors_re = project_authors(args.authors)

    files = []
    for p in args.paths:
        if os.path.isdir(p):
            files += sorted(os.path.join(p, f) for f in os.listdir(p) if f.endswith(".rst"))
        else:
            files.append(p)
    if args.ranges:
        for f in files:
            new = sorted(new_lines(f, args.authors_re)[0])
            spans, start = [], None
            for a, b in zip([None] + new, new + [None]):
                if start is None:
                    start = b
                elif b is None or b != a + 1:
                    spans.append(str(start) if start == a else "%d-%d" % (start, a))
                    start = b
            print("%s: %s" % (f, ", ".join(spans) or "none"))
        return 0
    names = {rid: name for rid, name, _, _ in RULES}
    results = [lint(f, args) for f in files]
    tot_words = tot_score = 0
    for r in results:
        tot_words += r["words"]
        tot_score += r["score"] * r["words"] / 1000.0
        share = "" if r["new_share"] is None else ", %d%% of lines new" % (100 * r["new_share"])
        counts = " ".join("%s:%d" % (k, len(v)) for k, v in r["hits"].items() if v)
        print("%-32s %6d words  score %5.1f/1000%s" % (r["path"], r["words"], r["score"], share))
        if counts:
            print("    " + counts)
        if args.verbose:
            print("    register: %d contractions, %d spelled out" % (r["contracted"], r["spelled"]))
            print("    rhythm: " + rhythm(r["lens"]))
            for rid, found in r["hits"].items():
                for lineno, snip in found:
                    print("    %s L%-5d %-12s %s" % (rid, lineno, names[rid][:12],
                                                     " ".join(snip.split())))
    if len(results) > 1:
        print("%-32s %6d words  score %5.1f/1000" % (
            "TOTAL", tot_words, 1000.0 * tot_score / max(1, tot_words)))


if __name__ == "__main__":
    sys.exit(main())
