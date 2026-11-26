#!/usr/bin/env python3
"""
Print the book's covers, front and back, in colour, for a printer.

    scripts/book/cover.py --out-dir DIR            the two covers
    scripts/book/cover.py --out-dir DIR --draft    with a box where a painting
                                                   still to come goes

writes DIR/zangbandtk-cover-front.pdf and DIR/zangbandtk-cover-back.pdf, each
one page of 216 x 303 mm: A4 with 3 mm of bleed on every side, the printer's
trim marked by the PDF's TrimBox, as archive/books/PRINT_SPEC.md describes
for the Amber book.

The paintings are art/cover-front.jpg, which fills the front to its edges,
and art/cover-back.jpg, the small picture on the back (see
archive/design_brief_book/cover).  They are painted with no words on them;
the title and everything else on the covers is set here, in the book's own
fonts.  The facts on the back are counted from lib/gamedata each time, so
they don't go stale.

Needs Chrome or Chromium, as build.py does.
"""

import argparse
import base64
import glob
import html
import os
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import build  # noqa: E402  (fonts_css, version, find_chrome, print_pdf)

GAMEDATA = os.path.join(build.ROOT, "lib", "gamedata")

# The page, in mm: A4 and 3 mm of bleed round it.
BLEED = 3
WIDTH, HEIGHT = 210 + 2 * BLEED, 297 + 2 * BLEED


def count(pattern):
    """How many records the gamedata files matching pattern hold."""
    n = 0
    for path in glob.glob(os.path.join(GAMEDATA, pattern)):
        with open(path, encoding="utf-8") as f:
            n += sum(1 for line in f if line.startswith("name:"))
    return n


def picture(name, art_dir, draft, label):
    # A painting is kept as a JPEG: as a PNG it is several times the size
    # for nothing the eye can see in print.
    for ext, mime in [(".jpg", "image/jpeg"), (".png", "image/png")]:
        path = os.path.join(art_dir, name + ext)
        if os.path.exists(path):
            with open(path, "rb") as f:
                data = base64.b64encode(f.read()).decode("ascii")
            return '<img src="data:%s;base64,%s" alt="">' % (mime, data)
    if draft:
        return '<div class="todo">[%s] %s</div>' % (name, html.escape(label))
    sys.exit("cover.py: there is no %s.jpg in %s; --draft lays the cover "
             "out without it" % (name, art_dir))


CSS = """
@page { size: %(w)smm %(h)smm; margin: 0; }
html, body { margin: 0; padding: 0; }
body { -webkit-print-color-adjust: exact; print-color-adjust: exact; }
.cover { position: relative; width: %(w)smm; height: %(h)smm;
  overflow: hidden; }
/* Everything that must not be trimmed stays this far inside the trim. */
.safe { position: absolute; left: %(s)smm; right: %(s)smm; top: %(s)smm;
  bottom: %(s)smm; }
.todo { position: absolute; inset: 0; display: flex; align-items: center;
  justify-content: center; text-align: center; padding: 0 30mm;
  font-family: 'Book Serif', serif; font-style: italic; font-size: 12pt;
  color: #ccc; border: 1mm dashed #888; box-sizing: border-box; }

/* The front: the painting to the edges, the title over its sky. */
.front { background: #101828; }
.front > img { position: absolute; inset: 0; width: 100%%; height: 100%%;
  object-fit: cover; object-position: center top; }
.front .todo { background: linear-gradient(#2a3a6a, #6a3a2a); }
.front .title { position: absolute; top: 1mm; left: 0; right: 0;
  text-align: center; }
.front .name { font-family: 'Book Head', serif; font-weight: 700;
  font-size: 62pt; line-height: 1; letter-spacing: 0.01em;
  color: #ffd23a; -webkit-text-stroke: 1.6pt #1a0d00;
  paint-order: stroke fill;
  text-shadow: 0 1.2mm 0 #1a0d00, 0 2.5mm 4mm rgba(0,0,0,0.55); }
.front .what { margin-top: 2mm; font-family: 'Book Head', serif;
  font-weight: 700; font-size: 16pt; letter-spacing: 0.12em;
  text-transform: uppercase; color: #fff; -webkit-text-stroke: 0.8pt #1a0d00;
  paint-order: stroke fill; text-shadow: 0 1mm 3mm rgba(0,0,0,0.8); }
.front .foot { position: absolute; bottom: 0; left: 0; right: 0;
  text-align: center; font-family: 'Book Serif', serif; font-style: italic;
  font-weight: 700; font-size: 12.5pt; line-height: 1.35; color: #fff;
  text-shadow: 0 0 2mm #000, 0 0 4mm #000; }

/* The back: the colour of an old rulebook's back cover, the blurb, and the
   small painting in a box. */
.back { background: #8e1c12; color: #160604; }
.back .head { font-family: 'Book Head', serif; font-weight: 700;
  font-size: 52pt; line-height: 1; color: #160604; letter-spacing: 0.01em; }
.back .sub { font-family: 'Book Head', serif; font-weight: 700;
  font-size: 18pt; margin-top: 2.5mm; letter-spacing: 0.08em;
  text-transform: uppercase; color: #f2c14a; }
.back .rule { border-top: 1.2pt solid #160604; margin: 6mm 0 7mm; }
.back .body { display: flex; flex-direction: column; gap: 9mm; }
.back ul { flex: 1; margin: 0; padding: 0; list-style: none;
  font-family: 'Book Serif', serif; font-size: 14.6pt; line-height: 19.5pt;
  color: #fff6e6; }
.back li { margin: 0 0 6.5mm; padding-left: 5mm; position: relative; }
.back li::before { content: "\\2022"; position: absolute; left: 0;
  color: #f2c14a; }
.back li b { color: #ffd23a; }
.back .box { flex: none; width: 84mm; height: 84mm; position: relative;
  align-self: flex-end; margin-top: -2mm;
  border: 1.2mm solid #160604; background: #160604; }
.back .box img { width: 100%%; height: 100%%; object-fit: cover;
  display: block; }
.back .end { position: absolute; left: 0; right: 0; bottom: 0;
  font-family: 'Book Serif', serif; color: #fff6e6; }
.back .site { font-family: 'Book Head', serif; font-weight: 700;
  font-size: 20pt; color: #ffd23a; }
.back .free { font-size: 10.5pt; line-height: 13.5pt; margin-top: 3mm; }
.back .small { font-size: 8.4pt; line-height: 11pt; margin-top: 3mm;
  color: #f5d9c8; }
"""


def front(art_dir, draft):
    return """
<section class="cover front">
  %s
  <div class="safe">
    <div class="title">
      <div class="name">ZangbandTK</div>
      <div class="what">The Players' Manual</div>
    </div>
    <div class="foot">A game of the dungeons, the wilderness<br>and the
      Courts of Amber</div>
  </div>
</section>""" % picture("cover-front", art_dir, draft,
                        "The front cover's painting goes here")


def back(art_dir, draft, ver):
    races, classes = count("p_race.txt"), count("class.txt")
    monsters = count("monster.txt") + count("monster.*.txt")
    artifacts = count("artifact*.txt")
    towns, dungeons = count("town.txt"), count("dungeon.txt")
    # Rounded down to the hundred below, so the cover stays true as the
    # bestiary grows.
    monsters_rounded = monsters // 100 * 100
    points = [
        "<b>%d races and %d classes</b>, from elves, dwarves and hobbits "
        "to Amberites, vampires, golems and the walking dead." % (
            races, classes),
        "<b>Seven realms of magic</b>, from the healing of Life to the "
        "Trump cards that open a way to anywhere.",
        "<b>A world above the dungeons</b>: %d towns, %d dungeons, the "
        "wilderness between them, and work to be had at the inns." % (
            towns, dungeons),
        "<b>Over {:,} monsters</b> and %d artifacts, the princes and "
        "princesses of Amber among them.".format(monsters_rounded) % artifacts,
        "<b>One way to win</b>: go down through the Courts of Chaos, and "
        "face Oberon at the ninety-ninth level and the Serpent of Chaos at "
        "the hundredth.",
    ]
    return """
<section class="cover back">
  <div class="safe" style="left: 18mm; right: 18mm; top: 20mm; bottom: 18mm;">
    <div class="head">ZangbandTK</div>
    <div class="sub">The Players' Manual</div>
    <div class="rule"></div>
    <div class="body">
      <ul>%s</ul>
      <div class="box">%s</div>
    </div>
    <div class="end">
      <div class="site">zangbandtk.com</div>
      <div class="free">ZangbandTK is free, for Mac, Windows and Linux.
        This book is the game's own manual, version %s.</div>
      <div class="small">Built on Zangband and Angband, by their developers
        past and present. Under the Angband licence this book may be copied
        and given away, but not sold.</div>
    </div>
  </div>
</section>""" % ("".join("<li>%s</li>" % p for p in points),
                 picture("cover-back", art_dir, draft,
                         "The back cover's small painting"),
                 html.escape(ver))


def document(body):
    css = CSS % {"w": WIDTH, "h": HEIGHT, "s": BLEED + 9}
    return ("<!doctype html><html><head><meta charset='utf-8'>"
            "<style>%s%s</style></head><body>%s</body></html>"
            % (build.fonts_css(), css, body))


def set_trim(pdf):
    """Mark the trim, 3 mm in from each edge, with a TrimBox and BleedBox.

    Chrome writes neither.  Its PDFs keep their page objects and the
    cross-reference table uncompressed, so the boxes go in beside the
    MediaBox and build.rewrite_xref writes the table again.
    """
    with open(pdf, "rb") as f:
        data = f.read()
    media = re.findall(rb"/MediaBox\s*\[\s*([\d.]+)\s+([\d.]+)\s+"
                       rb"([\d.]+)\s+([\d.]+)\s*\]", data)
    if len(media) != 1:
        sys.exit("cover.py: expected one page in %s, found %d"
                 % (pdf, len(media)))
    # The page as Chrome printed it, which rounds the size a little.
    x0, y0, x1, y1 = [float(v) for v in media[0]]
    b = BLEED * 72 / 25.4
    boxes = (b"/TrimBox [%.3f %.3f %.3f %.3f] /BleedBox [%.3f %.3f %.3f %.3f] "
             % (x0 + b, y0 + b, x1 - b, y1 - b, x0, y0, x1, y1))
    data = re.sub(rb"/MediaBox\s*\[", boxes + b"/MediaBox [", data)
    with open(pdf, "wb") as f:
        f.write(build.rewrite_xref(data, "cover.py"))


def main():
    p = argparse.ArgumentParser(description=__doc__.strip().split("\n\n")[0])
    p.add_argument("--out-dir", required=True)
    p.add_argument("--art", default=os.path.join(HERE, "art"))
    p.add_argument("--draft", action="store_true")
    p.add_argument("--html", help="also keep the pages Chrome prints, as "
                   "FILE-front.html and FILE-back.html")
    args = p.parse_args()
    os.makedirs(args.out_dir, exist_ok=True)
    ver = build.version()
    tmp = tempfile.mkdtemp(prefix="cover-")
    try:
        for side, body in [("front", front(args.art, args.draft)),
                           ("back", back(args.art, args.draft, ver))]:
            page = os.path.join(tmp, side + ".html")
            with open(page, "w", encoding="utf-8") as f:
                f.write(document(body))
            if args.html:
                shutil.copy(page, "%s-%s.html" % (args.html, side))
            out = os.path.abspath(os.path.join(
                args.out_dir, "zangbandtk-cover-%s.pdf" % side))
            build.print_pdf(page, out, os.path.join(tmp, "profile"))
            pages = build.page_count(out)
            if pages != 1:
                sys.exit("cover.py: the %s cover came out as %d pages"
                         % (side, pages))
            set_trim(out)
            print("Wrote %s" % out)
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
