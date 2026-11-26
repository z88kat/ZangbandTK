#!/usr/bin/env python3
"""Put a drawing for the book into scripts/book/art.

    scripts/book/import-art.py NAME PICTURE

NAME is the picture's name in art.txt; PICTURE is the drawing as the image
generator gave it (see archive/design_brief_book).  It is turned to black ink
on white: grey and colour are dropped, the paper is made pure white and the
ink pure black, the white round the drawing is trimmed, and it is made the
size the page prints it at, 300 dots to the inch, so the book's file isn't
larger than it needs to be.  A few greys are kept between the two so edges
stay smooth.

Uses ImageMagick's magick, as Pillow isn't installed everywhere this runs.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))

# The widest each shape prints, in dots at 300 to the inch: a column is
# 85 mm, both columns 172 mm, the title page's picture 140 mm.
WIDTH = {"column": 1000, "spread": 2030, "square": 1650}


def shape(name):
    with open(os.path.join(HERE, "art.txt"), encoding="utf-8") as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            fields = line.rstrip("\n").split("\t")
            if fields[0] == name:
                place = fields[2]
                if place == "span":
                    return "square"
                return "spread" if place.endswith("span") else "column"
    sys.exit("import-art.py: art.txt has no picture called %s" % name)


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__.strip().split("\n\n")[1])
    name, picture = sys.argv[1], sys.argv[2]
    width = WIDTH[shape(name)]
    out_dir = os.path.join(HERE, "art")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, name + ".png")
    subprocess.run([
        "magick", picture,
        "-colorspace", "Gray",
        # Paper lighter than 88% is white and ink darker than 25% black.
        "-level", "25%,88%",
        # White round the drawing goes, and 2% of the width comes back.
        "-fuzz", "3%", "-trim", "+repage",
        "-resize", "%dx>" % (width - 2 * (width // 50)),
        "-bordercolor", "white", "-border", "%d" % (width // 50),
        "-posterize", "6",
        "-define", "png:color-type=0", "-depth", "8",
        "-strip", out], check=True)
    size = subprocess.run(["magick", "identify", "-format", "%wx%h", out],
                          capture_output=True, text=True, check=True).stdout
    print("%s: %s, %s, %d KB" % (out, shape(name), size,
                                  os.path.getsize(out) // 1024))


if __name__ == "__main__":
    main()
