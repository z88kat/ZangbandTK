#!/usr/bin/env python3
"""
Print the manual as a book: an A4 rulebook laid out the way role-playing
games were in the early 1980s.

    scripts/book/build.py --out FILE.pdf             the book
    scripts/book/build.py --out FILE.pdf --draft     with a box where each
                                                     picture still to come goes
    scripts/book/build.py --html FILE.html           the page Chrome prints

    --game GAME    a game built with the spoiler front end
                   (-DSUPPORT_SPOIL_FRONTEND=ON), to write the treasure list
                   and the bestiary; without it the book has neither

The text is the manual's own.  docs/_ext/book.py, a Sphinx builder, writes
the chapters that conf.py's book_sections names, in that order, and this puts
them in the book: a title page and its reverse, the contents, a running head
on every page ("Page 14 * Creating a Character", the page number on the
outside), the pictures, and book.css over all of it.  Chrome prints the
result.

It prints twice.  The first time is to find the page each chapter begins on,
which Chrome can't put in the contents itself; the second has the numbers in,
and enough blank pages at the end to make the count a multiple of four, as a
bound book needs.  Each chapter starts with a tiny white marker naming it,
and pdftotext finds the markers in the first printing.

Two chapters aren't the manual's: the treasure list, after the chapter on
objects, and the bestiary, after the one on monsters.  compendium.py writes
them from the game's data and its spoiler files, so they need --game.

The pictures are listed in art.txt, one a line: the picture's name, the
chapter, where in it, and what it shows.  The picture itself is ART/<name>.png
(ART is art/ here unless --art says otherwise), in black and white.  One
that isn't there yet is left out, or with --draft drawn as a box saying what
goes there, so the pages can be laid out before the drawings exist.

Needs Sphinx and Pyphen (docs/requirements.txt; .venv-docs is used for
Sphinx when there is one, but run this with its Python for Pyphen),
Chrome or Chromium (set CHROME if it isn't found), and pdftotext and pdfinfo
from poppler.

The fonts are in fonts/: TeX Gyre Schola, Bonum and Cursor, the GUST
project's free versions of Century Schoolbook, Bookman and Courier, under the
GUST Font License (fonts/GUST-FONT-LICENSE.txt).  The PDF carries what it
uses of them.
"""

import argparse
import base64
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

import compendium

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))

FONTS = [
    ("Book Serif", "texgyreschola-regular.otf", 400, "normal"),
    ("Book Serif", "texgyreschola-italic.otf", 400, "italic"),
    ("Book Serif", "texgyreschola-bold.otf", 700, "normal"),
    ("Book Serif", "texgyreschola-bolditalic.otf", 700, "italic"),
    ("Book Head", "texgyrebonum-regular.otf", 400, "normal"),
    ("Book Head", "texgyrebonum-bold.otf", 700, "normal"),
    ("Book Type", "texgyrecursor-regular.otf", 400, "normal"),
    ("Book Type", "texgyrecursor-bold.otf", 700, "normal"),
]

MARK = "ZZBOOKCHAPTERZZ"


# --- the chapters

def sphinx():
    venv = os.path.join(ROOT, ".venv-docs", "bin", "sphinx-build")
    if os.access(venv, os.X_OK):
        return venv
    path = shutil.which("sphinx-build")
    if not path:
        raise SystemExit("build.py: no sphinx-build.  Install "
                         "docs/requirements.txt, or make .venv-docs.")
    return path


def chapters(tmp):
    out = os.path.join(tmp, "sphinx")
    subprocess.run([sphinx(), "-E", "-q", "-b", "book",
                    "-d", os.path.join(tmp, "doctrees"),
                    os.path.join(ROOT, "docs"), out], check=True)
    with open(os.path.join(out, "book.html"), encoding="utf-8") as f:
        body = f.read()
    with open(os.path.join(out, "book.json"), encoding="utf-8") as f:
        info = json.load(f)
    return body, info


def version():
    with open(os.path.join(ROOT, "src", "buildid.h"), encoding="utf-8") as f:
        return re.search(r'#define VERSION_STRING\s+"([^"]+)"',
                         f.read()).group(1)


# What goes after which chapter, from compendium.py.
COMPENDIUM = [("objects", compendium.TREASURE),
              ("monsters", compendium.BESTIARY)]


def add_compendium(body, info, game, tmp):
    """The treasure list and the bestiary, each after the manual's chapter
    on the same thing, in the contents and with running heads."""
    made = compendium.chapters(game, tmp)
    for host, doc in COMPENDIUM:
        start = body.find('<section class="chapter" id="%s--"' % host)
        if start < 0:
            continue
        after = body.find('<section class="chapter', start + 1)
        if after < 0:
            after = len(body)
        body = body[:after] + made[doc] + body[after:]
        for _, docs in info["sections"]:
            if host in docs:
                docs.insert(docs.index(host) + 1, doc)
        info["titles"][doc] = compendium.TITLES[doc]
    return body


# --- the pictures

def read_art():
    """[(name, chapter, place, subject)] from art.txt."""
    art = []
    with open(os.path.join(HERE, "art.txt"), encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line.strip() or line.startswith("#"):
                continue
            name, doc, place, subject = [p.strip() for p in
                                         line.split("\t") if p.strip()][:4]
            art.append((name, doc, place, subject))
    return art


def figure(name, place, subject, art_dir, draft):
    span = " span" if place.endswith("span") else ""
    path = os.path.join(art_dir, name + ".png")
    if os.path.exists(path):
        with open(path, "rb") as f:
            data = base64.b64encode(f.read()).decode("ascii")
        return ('<figure class="art%s"><img alt="%s" '
                'src="data:image/png;base64,%s"></figure>'
                % (span, html.escape(subject), data))
    if draft:
        return ('<figure class="art draft%s">[%s] %s</figure>'
                % (span, name, html.escape(subject)))
    return ""


def place_art(body, info, art_dir, draft):
    for name, doc, place, subject in read_art():
        if doc == "title":
            continue
        fig = figure(name, place, subject, art_dir, draft)
        if not fig:
            continue
        where, _, anchor = place.partition(":")
        if where in ("open", "open-span"):
            # After the chapter's title.
            m = re.search(r'(<section class="chapter" id="%s--".*?</h1>)'
                          % re.escape(doc), body, re.S)
        else:
            # After the heading of the section with that id, which may have
            # the targets of other names for it in front of it.
            m = re.search(r'id="%s--%s">\s*(?:<span id="[^"]*"></span>)*'
                          r'\s*(<h[2-6][^>]*>.*?</h[2-6]>)'
                          % (re.escape(doc), re.escape(anchor.replace(
                              "-span", ""))), body, re.S)
        if not m:
            print("build.py: art.txt puts %s at %s in %s, which isn't there"
                  % (name, place, doc), file=sys.stderr)
            continue
        if where in ("open", "open-span"):
            body = body[:m.end()] + fig + body[m.end():]
        else:
            # The heading and its picture go together, so the heading is
            # never left at the foot of a column with its picture in the
            # next.  Chrome doesn't keep a heading with what follows it.
            span = " span" if place.endswith("span") else ""
            body = (body[:m.start(1)] + '<div class="headed%s">' % span
                    + m.group(1) + fig + "</div>" + body[m.end(1):])
    return body


def title_art(art_dir, draft):
    for name, doc, place, subject in read_art():
        if doc == "title":
            return figure(name, "span", subject, art_dir, draft)
    return ""


# --- the book's own pages

def css_string(text):
    return '"%s"' % text.replace("\\", "\\\\").replace('"', '\\"')


def running_heads(info):
    rules = []
    for doc, title in info["titles"].items():
        t = css_string(title)
        rules.append(
            '@page ch-%s:left { @top-left { content: "Page " counter(page) '
            '"  \\2022  " %s; } @top-center { content: ""; } '
            '@top-right { content: ""; } }\n'
            '@page ch-%s:right { @top-right { content: %s "  \\2022  Page " '
            'counter(page); } @top-center { content: ""; } '
            '@top-left { content: ""; } }\n'
            '.chapter[data-doc="%s"] { page: ch-%s; }\n'
            % (doc, t, doc, t, doc, doc))
    # The rule under the head is the bottom border of all three boxes along
    # the top, so it runs the width of the page whichever has the words.
    box = ("font-family: 'Book Serif', serif; font-weight: 700; "
           "font-size: 9.4pt; vertical-align: bottom; padding-bottom: 1.2mm; "
           "margin-bottom: 5mm; border-bottom: 0.6pt solid #111;")
    common = ("@page { @top-left { %s text-align: left; } "
              "@top-center { %s } "
              "@top-right { %s text-align: right; } }\n"
              "@page front { @top-left { content: none; border: none; } "
              "@top-center { content: none; border: none; } "
              "@top-right { content: none; border: none; } }\n"
              % (box, box, box))
    return common + "".join(rules)


def fonts_css():
    out = []
    for family, file, weight, style in FONTS:
        with open(os.path.join(HERE, "fonts", file), "rb") as f:
            data = base64.b64encode(f.read()).decode("ascii")
        out.append("@font-face { font-family: '%s'; font-weight: %d; "
                   "font-style: %s; src: url(data:font/otf;base64,%s) "
                   "format('opentype'); }\n" % (family, weight, style, data))
    return "".join(out)


def front_matter(info, ver, pages, art):
    title = (
        '<section class="front titlepage">'
        '<div class="name">ZangbandTK</div>'
        '<div class="what">The Players\' Manual</div>'
        '<div class="rule"></div>'
        '<div class="art">%s</div>'
        '<div class="based">A game of the dungeons, the wilderness and the '
        'Courts of Amber<br>built on <b>Zangband</b> and <b>Angband 4.2</b>'
        '</div>'
        '<div class="ver">Version %s</div>'
        '</section>' % (art, html.escape(ver)))
    verso = (
        '<section class="front verso">'
        '<p>This book is the ZangbandTK manual, as it stands in version %s, '
        'set as a book.  The manual itself is at zangbandtk.com, and so is '
        'everything this book leaves out.</p>'
        '<p class="lic">ZangbandTK is available under the Angband licence: '
        'Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. '
        'Koeneke.  This software may be copied and distributed for '
        'educational, research, and not for profit purposes provided that '
        'this copyright and statement are included in all such copies.  '
        'Other copyrights may also apply.</p>'
        '<p>Set in TeX Gyre Schola, Bonum and Cursor.</p>'
        '</section>' % html.escape(ver))
    lines = ['<section class="front contents"><h1>Contents</h1>']
    for part, docs in info["sections"]:
        if not docs:
            continue
        lines.append('<div class="part">%s</div>' % html.escape(part))
        for doc in docs:
            n = pages.get(doc, "000") if pages else "000"
            lines.append('<a class="line" href="#%s--"><span class="t">%s'
                         '</span><span class="dots"></span>'
                         '<span class="n">%s</span></a>'
                         % (doc, html.escape(info["titles"][doc]), n))
    lines.append("</section>")
    return title + verso + "".join(lines)


def tidy(body, info):
    """What the site's HTML needs changing for a page."""
    # A table of four columns or more is too wide for one; it goes across
    # both.
    def wide(m):
        table = m.group(0)
        if 'class="list' in table.split(">")[0]:
            return table
        first = table.split("</tr>")[0]
        cols = len(re.findall(r"<t[hd]\b", first))
        if cols >= 4:
            table = table.replace("<table", '<table data-wide=""', 1)
        return table

    body = re.sub(r"<table\b.*?</table>", wide, body, flags=re.S)
    body = body.replace('<table data-wide="" class="', '<table class="wide ')

    # Each chapter's section of the contents, above its title, and the
    # marker that finds its page.
    part_of = {d: p for p, docs in info["sections"] for d in docs}

    def opener(m):
        doc = m.group(1)
        return ('%s<div class="marker">%s%s%s</div>'
                '<p class="kicker">%s</p>'
                % (m.group(0), MARK, doc.replace("-", "QQ"), MARK,
                   html.escape(part_of.get(doc, ""))))

    body = re.sub(r'<section class="chapter[^"]*" id="([^"]+)--"[^>]*>',
                  opener, body)
    return body


# Hyphenation is done here, not by Chrome: Chrome hyphenates on a Mac but not
# on Linux, where the site's copy is printed, and two justified columns
# without it open up in rivers.  Pyphen has LibreOffice's patterns; each
# word gets a soft hyphen at every point they allow, and book.css says
# "hyphens: manual", so the book breaks the same wherever it is made.
NO_HYPHENS = {"code", "kbd", "pre", "tt", "samp", "script", "style", "title",
              "th", "h1", "h2", "h3", "h4", "h5", "h6"}
# A word of six letters or more, all small after the first, and not part of
# an address, an entity or a name with a hyphen in it.
WORD = re.compile(r"(?<![\w&#/.@-])[A-Za-z][a-z]{5,}"
                  r"(?![\w/@-]|\.[A-Za-z])")


def hyphenate(body):
    try:
        import pyphen
    except ImportError:
        raise SystemExit("build.py: no pyphen.  Install docs/requirements.txt"
                         ", or run with .venv-docs/bin/python.")
    dic = pyphen.Pyphen(lang="en_GB", left=2, right=3)
    seen = {}

    def word(m):
        w = m.group(0)
        if w not in seen:
            seen[w] = dic.inserted(w, hyphen="\u00ad")
        return seen[w]

    out, skip = [], []
    for part in re.split(r"(<[^>]*>)", body):
        if part.startswith("<"):
            m = re.match(r"<(/?)([a-zA-Z0-9]+)", part)
            if m and m.group(2).lower() in NO_HYPHENS:
                if m.group(1):
                    if skip:
                        skip.pop()
                elif not part.endswith("/>"):
                    skip.append(m.group(2))
            out.append(part)
        elif skip:
            out.append(part)
        else:
            out.append(WORD.sub(word, part))
    return "".join(out)


def page(info, body, ver, pages, blanks, art_dir, draft):
    css = open(os.path.join(HERE, "book.css"), encoding="utf-8").read()
    return ('<!doctype html><html lang="en-GB"><head><meta charset="utf-8">'
            '<title>ZangbandTK: The Players\' Manual</title>'
            '<style>%s%s%s</style></head><body>%s%s%s</body></html>'
            % (fonts_css(), css, running_heads(info),
               front_matter(info, ver, pages, title_art(art_dir, draft)),
               body, '<div class="blank"></div>' * blanks))


# --- printing

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


def print_pdf(page_path, pdf, profile):
    # In CI, and in a container as root, Chrome's sandbox can't start; the
    # page is our own, so print without it there.
    root = hasattr(os, "geteuid") and os.geteuid() == 0
    sandbox = ["--no-sandbox"] if os.environ.get("CI") or root else []
    subprocess.run([find_chrome(), "--headless=new", "--disable-gpu"]
                   + sandbox
                   + ["--no-first-run", "--no-default-browser-check",
                      "--disable-extensions", "--user-data-dir=" + profile,
                      "--no-pdf-header-footer", "--print-to-pdf-no-header",
                      "--generate-pdf-document-outline",
                      "--virtual-time-budget=60000",
                      "--print-to-pdf=" + pdf, "file://" + page_path],
                   check=True, capture_output=True)


def rewrite_xref(data, who):
    """data with its cross-reference table written again, for objects that
    have moved.  Chrome's PDFs keep the table, and every object, outside
    any compressed stream, so this is all a change to one needs."""
    xref = data.rfind(b"\nxref\n")
    if xref < 0:
        raise SystemExit("%s: the PDF has no cross-reference table to "
                         "rewrite" % who)
    body = data[:xref + 1]
    offsets = {int(m.group(1)): m.start(1)
               for m in re.finditer(rb"(?m)^(\d+) 0 obj\b", body)}
    size = max(offsets) + 1
    table = [b"xref\n0 %d\n0000000000 65535 f \n" % size]
    for i in range(1, size):
        if i in offsets:
            table.append(b"%010d 00000 n \n" % offsets[i])
        else:
            table.append(b"0000000000 65535 f \n")
    trailer = data[data.index(b"trailer", xref):]
    trailer = re.sub(rb"startxref\s+\d+",
                     b"startxref\n%d" % (xref + 1), trailer)
    return body + b"".join(table) + trailer


# Chrome puts every soft hyphen into the PDF's text, though it only shows
# the ones it breaks a line at.  Each is a span whose ActualText is U+00AD,
# empty or round an invisible glyph of no width; a hyphen that breaks a line
# is drawn after it, on its own.  Left in, they split words for anything that
# reads the text: Preview finds "dungeon" 19 times in the book instead of
# 239.  So each span goes, keeping only the move (Td) that places what comes
# after it, and so does U+00AD inside a ligature's ActualText ("f\u00adf"
# drawn as one ff).
SOFT_HYPHEN = re.compile(rb"/Span\s*<<\s*/ActualText\s*<FEFF00AD>\s*>>\s*BDC"
                         rb"\s*(?:([-\d.]+\s+[-\d.]+\s+Td)\s*)?"
                         rb"(?:<[0-9A-Fa-f]*>\s*Tj\s*)?EMC\s*")
LIGATURE_TEXT = re.compile(rb"/ActualText\s*<FEFF((?:[0-9A-Fa-f]{4}){2,})>")
STREAM = re.compile(rb"(?m)^\d+ 0 obj\s*(<<(?:(?!endobj).)*?>>)\s*stream\r?\n",
                    re.S)


def strip_soft_hyphens(pdf):
    import zlib

    def ligature(m):
        units = [m.group(1)[i:i + 4] for i in range(0, len(m.group(1)), 4)]
        units = [u for u in units if u.upper() != b"00AD"]
        return b"/ActualText <FEFF" + b"".join(units) + b">"

    with open(pdf, "rb") as f:
        data = f.read()
    out, pos, changed = [], 0, 0
    for m in STREAM.finditer(data):
        head = m.group(1)
        length = re.search(rb"/Length (\d+)", head)
        if not length or b"/FlateDecode" not in head:
            continue
        start = m.end()
        end = start + int(length.group(1))
        try:
            text = zlib.decompress(data[start:end])
        except zlib.error:
            continue
        if b"00AD" not in text.upper():
            continue
        new = SOFT_HYPHEN.sub(
            lambda h: h.group(1) + b"\n" if h.group(1) else b"", text)
        new = LIGATURE_TEXT.sub(ligature, new)
        if new == text:
            continue
        packed = zlib.compress(new, 9)
        out.append(data[pos:m.start(1)])
        out.append(head.replace(length.group(0),
                                b"/Length %d" % len(packed)))
        out.append(data[m.end(1):start])
        out.append(packed)
        pos = end
        changed += 1
    if changed:
        out.append(data[pos:])
        with open(pdf, "wb") as f:
            f.write(rewrite_xref(b"".join(out), "build.py"))


def page_count(pdf):
    out = subprocess.run(["pdfinfo", pdf], check=True, capture_output=True,
                         text=True).stdout
    return int(re.search(r"Pages:\s+(\d+)", out).group(1))


def chapter_pages(pdf):
    text = subprocess.run(["pdftotext", "-raw", pdf, "-"], check=True,
                          capture_output=True, text=True).stdout
    pages = {}
    for n, sheet in enumerate(text.split("\f"), 1):
        for doc in re.findall(MARK + r"(\S+?)" + MARK, sheet):
            pages.setdefault(doc.replace("QQ", "-"), n)
    return pages


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0].strip())
    ap.add_argument("--out", help="the PDF to write")
    ap.add_argument("--html", help="write the page Chrome prints, and stop")
    ap.add_argument("--draft", action="store_true",
                    help="a box for each picture still to come")
    ap.add_argument("--art", default=os.path.join(HERE, "art"),
                    help="where the pictures are")
    ap.add_argument("--game", help="a game built with the spoiler front end, "
                    "for the treasure list and the bestiary")
    args = ap.parse_args()
    if not args.out and not args.html:
        ap.error("say where: --out or --html")

    ver = version()
    with tempfile.TemporaryDirectory() as tmp:
        body, info = chapters(tmp)
        if args.game:
            body = add_compendium(body, info, os.path.abspath(args.game), tmp)
        else:
            print("build.py: no --game, so no treasure list or bestiary",
                  file=sys.stderr)
        body = tidy(body, info)
        body = hyphenate(body)
        body = place_art(body, info, args.art, args.draft)

        if args.html:
            with open(args.html, "w", encoding="utf-8") as f:
                f.write(page(info, body, ver, None, 0, args.art, args.draft))
            return 0

        path = os.path.join(tmp, "book.html")
        pdf = os.path.join(tmp, "first.pdf")
        profile = os.path.join(tmp, "profile")

        with open(path, "w", encoding="utf-8") as f:
            f.write(page(info, body, ver, None, 0, args.art, args.draft))
        print_pdf(path, pdf, profile)
        pages = chapter_pages(pdf)
        count = page_count(pdf)
        lost = [d for _, docs in info["sections"] for d in docs
                if d not in pages]
        if lost:
            print("build.py: can't find where %s begin%s"
                  % (", ".join(lost), "s" if len(lost) == 1 else ""),
                  file=sys.stderr)
            return 1

        blanks = (4 - count % 4) % 4
        with open(path, "w", encoding="utf-8") as f:
            f.write(page(info, body, ver, pages, blanks, args.art,
                         args.draft))
        out = os.path.abspath(args.out)
        os.makedirs(os.path.dirname(out), exist_ok=True)
        print_pdf(path, out, profile)
        strip_soft_hyphens(out)

        again = chapter_pages(out)
        final = page_count(out)
        if again != pages:
            print("build.py: the chapters moved between printings",
                  file=sys.stderr)
            return 1
        if final % 4:
            print("build.py: %d pages, which isn't a multiple of four"
                  % final, file=sys.stderr)
            return 1
    print("Wrote %s: %d pages" % (args.out, final))
    return 0


if __name__ == "__main__":
    sys.exit(main())
