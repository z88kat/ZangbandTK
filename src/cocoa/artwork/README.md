# Application icon artwork

`ZangbandTK_Icons.icns` is built from these, and they are kept so it can be
rebuilt or altered without starting over.

| File | What it is |
|---|---|
| `zangbandtk-icon-1024.png` | The icon as shipped: the logo inset on a rounded square, transparent surround, macOS style. |
| `zangbandtk-icon-square-1024.png` | The same artwork full-bleed, as the original was. Swap this in if the square is preferred. |

## Where it came from

Zangband's own macOS icon, `archive/zangband/src/zangband.icns` — a white disc
carrying a black device of two mirrored serpents with a cross through them, on a
crimson field.

The largest image in that file is 128×128, and it had been through an 8-bit
palette conversion at some point: 251 colours where there should have been three,
most of them dither noise, with a red fringe scattered along the boundaries
between the black device and the white disc.

## How it was cleaned

1. Extract the 128×128 image (`sips`; ImageMagick cannot decode that vintage of
   icns).
2. Remap every pixel to the nearest of the three true colours — black, white,
   crimson `#DE0029` — which removes the dither.
3. Separate the three colours into masks. Red then has its connected components
   measured: the four corner regions are the real background, and everything
   smaller is fringe, which is folded into the white disc.
4. Upscale each mask 8× with nearest-neighbour, blur, and re-threshold at 50%.
   That turns the staircased pixel edges into smooth curves without inventing
   detail.
5. Composite in layer order — crimson field, then the whole non-red area filled
   white, then the black device over it. Filling the disc white *before* the
   device matters: compositing white and black as separate masks leaves gaps
   between them where the red field shows through as a seam.
6. Inset on a rounded square, and build the iconset with all ten sizes macOS
   wants (16 through 512, each with its @2x).

## Rebuilding

Requires ImageMagick (`brew install imagemagick`) and `iconutil`, which ships
with macOS.

    magick zangbandtk-icon-1024.png -resize 512x512 icon_512x512.png   # &c.
    iconutil -c icns ZangbandTK.iconset -o ZangbandTK_Icons.icns
