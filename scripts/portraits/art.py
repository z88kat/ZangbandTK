"""The portraits' art: every layer, every race and every class.

Read by build-portraits.py, which says how they are put together.  Each layer
is (dx, dy, rows): a grid of letters placed dx, dy from an anchor -- the head's
top left for a head's layers, the canvas's for everything else.  "." is
nothing.  Letters are roles, given colours by the race's palette in a race's
layers and by the class's in a class's, each falling back on the other: so a
hood's "S" is the face's own shadow, and a Klackon's "K" stays its own whatever
a class calls K.

    s S k   skin, its shadow, its highlight     h H j   hair
    w E     eye white, iris                     b       brow
    m l     mouth, lips                         u U z   muzzle, its shadow, nose
    t T     teeth or tentacles                  R Q     horn
    x X     wing, its spar                      v       war paint
    c C     cloth, its shadow                   d D     trim
    p P q   metal, shadow, highlight            g G     glow
    W V     blade, its edge                     n N     wood
    i       tabard or undershirt                r y     device
    e       an edge drawn by hand, where the outline cannot reach
    o O     hood                                f F     fletching
    a A K M Klackon: carapace, shadow, highlight, mandible
    J Z     Amberite: the jewel, its chain

To add a race or a class, add an entry below; build-portraits.py fails until
every race and class in lib/gamedata has one.
"""


# --- heads ---------------------------------------------------------------------
#
# Twelve wide and twelve tall at the head's anchor, apart from the Mindflayer's,
# which is bigger and says where it starts.

HEADS = {
    "round": (0, 0, [
        "...ssssss...",
        "..sksssssS..",
        ".sksssssssS.",
        ".sssssssssS.",
        ".sbbssssbbS.",
        ".swEssssEwS.",
        ".sssssssssS.",
        ".sssskSsssS.",
        ".sssssssssS.",
        "..sssmmssS..",
        "...sssssS...",
        "....SSSS....",
    ]),
    "roundf": (0, 0, [
        "...ssssss...",
        "..sksssssS..",
        ".sksssssssS.",
        ".sssssssssS.",
        ".sbssssssbS.",
        ".swEssssEwS.",
        ".sssssssssS.",
        ".sssskSsssS.",
        "..sssssssS..",
        "..sssllssS..",
        "....sssS....",
        ".....SS.....",
    ]),
    "square": (0, 0, [
        "..ssssssss..",
        ".skssssssssS",
        ".sksssssssSS",
        ".sssssssssSS",
        ".sbbbssbbbSS",
        ".swEssssEwSS",
        ".sssssssssSS",
        ".sssskSsssSS",
        ".sssssssssSS",
        ".sssmmmmssSS",
        ".sssssssssS.",
        "..SSSSSSSS..",
    ]),
    # A Half-Troll's: a ridge for a brow, eyes too small for the face, a nose
    # that has been broken, and tusks.
    "brute": (0, 0, [
        "..ssssssss..",
        ".sksssssssS.",
        ".skssssszsS.",
        ".sssssssssSS",
        "sbbbbbbbbbbS",
        "ssEssssssESS",
        "sssskkSssSSS",
        "ssskkkSSsSSS",
        "sssssssssSSS",
        "sTsmmmmmsTSS",
        "ssssssssssSS",
        ".SSSSSSSSSS.",
    ]),
    # Dog-headed, lizard-headed or beast-headed: a muzzle, and a nose on it.
    "snout": (0, 0, [
        "...ssssss...",
        "..sksssssS..",
        ".sksssssssS.",
        ".sssssssssS.",
        ".sbbssssbbS.",
        ".sEwssssEwS.",
        ".sssuuuussS.",
        ".ssuuzzuuSS.",
        ".ssuuuuuuSS.",
        "..suumUuuS..",
        "...uuuuUU...",
        "....SSSS....",
    ]),
    "skull": (0, 0, [
        "...ssssss...",
        "..sksssssS..",
        ".sksssssssS.",
        ".sssssssssS.",
        ".szzzsszzzS.",
        ".szEzsszEzS.",
        ".szzzsszzzS.",
        ".sssszzsssS.",
        "..ssssssss..",
        "..stmtmtmS..",
        "...stttsS...",
        "....SSSS....",
    ]),
    # A Mindflayer's: all mind.  Two wider and two taller than a head.
    "bulb": (-1, -2, [
        "....ssssss....",
        "..sksssssssS..",
        ".sksssssssssS.",
        "sksssssssssssS",
        "sssssssssssssS",
        "ssssssssssssSS",
        ".sssssssssssS.",
        ".sswwssssswwS.",
        "..sssssssssS..",
        "..ssssSsssSS..",
        "..sssssssssS..",
        "...sssssssS...",
        "....ssssSS....",
        "....SSSSSS....",
    ]),
    # A Golem's: dressed stone, a brow, and eyes lit from inside.
    "block": (0, 0, [
        ".ssssssssss.",
        "skksssssssSS",
        "sksssssszsSS",
        "ssssssssszSS",
        "sSSSSSSSSSSS",
        "ssgGssssgGSS",
        "ssssssssssSS",
        "ssszsssssSSS",
        "sssszssssSSS",
        "sssmmmmmsSSS",
        "ssssssssssSS",
        ".SSSSSSSSSS.",
    ]),
    "insect": (0, 0, [
        "...aaaaaa...",
        "..aKaaaaaA..",
        ".aKaaaaaaaA.",
        ".aaaaaaaaaA.",
        "EEaaaaaaaaEE",
        "EwEaaaaaaEwE",
        "EEEaaaaaaEEE",
        ".EaaaAAaaaE.",
        "..aaaaaaaA..",
        "..AaaaaaaA..",
        ".M.aaaaaA.M.",
        "..MM....MM..",
    ]),
}


# --- hair ----------------------------------------------------------------------

HAIR = {
    "short": (-1, -1, [
        "...hhhhhhhh...",
        "..hjjhhhhhhh..",
        ".hjhhhhhhhhhH.",
        ".hh.......hHH.",
        ".h..........H.",
    ]),
    "slick": (-1, -1, [
        "...hhhhhhhh...",
        "..hjhhhhhhhh..",
        ".hjhhhhhhhhhH.",
        ".hhh..hh..hHH.",
        ".hh........HH.",
        ".h..........H.",
    ]),
    "messy": (-1, -2, [
        "...h..h.h.h...",
        "..hhjhh.hhhh..",
        ".hjhhhhhhhhhH.",
        ".hh.hh..h.hHH.",
        ".h.h......h.H.",
    ]),
    "tuft": (3, -2, [
        "..hh..",
        ".hjhH.",
        ".hhhH.",
        "..hH..",
    ]),
    "curly": (-1, -2, [
        "....h.hh.h....",
        "..hjhhjhhjhh..",
        ".hjhhjhhjhhhH.",
        "hhjhhhhjhhhhHH",
        "hhh.h..h..hHHH",
        "hh..........HH",
    ]),
    "curlylong": (-1, -2, [
        "....h.hh.h....",
        "..hjhhjhhjhh..",
        ".hjhhjhhjhhhH.",
        "hhjhhhhjhhhhHH",
        "hhh.h..h..hHHH",
        "jh..........Hj",
        "hh..........HH",
        "jh..........Hj",
        "hj..........jH",
        "jh..........Hj",
        "hhj........jHH",
        "jhh........HHj",
        ".jh........Hj.",
    ]),
    "long": (-1, -1, [
        "...hhhhhhhh...",
        "..hjjhhhhhhh..",
        ".hjhhhhhhhhhH.",
        "hhhhh....hhhHH",
        "hhh.........HH",
        "hh..........HH",
        "hh..........HH",
        "hh..........HH",
        "hh..........HH",
        "hhh........HHH",
        "hhh........HHH",
        "hhhh......HHHH",
        "hhh........HHH",
        "hh..........HH",
        "h............H",
    ]),
    "stringy": (-1, -1, [
        "...hhhhhhhh...",
        "..hjhhhhhhhh..",
        ".hjh.hhh.hhhH.",
        "hhh........hHH",
        "h.h.........H.",
        "h.h........H.H",
        "h............H",
        "hh.........H.H",
        "h.h.........H.",
        "h............H",
        "hh..........HH",
        ".h..........H.",
        "h............H",
        ".h..........H.",
    ]),
    "braids": (-1, -1, [
        "...hhhhhhhh...",
        "..hjjhhhhhhh..",
        ".hjhhhhhhhhhH.",
        "hhhh......hHHH",
        "hh..........HH",
        "hh..........HH",
        "jh..........Hj",
        "hh..........HH",
        "jh..........Hj",
        "hh..........HH",
        "jh..........Hj",
        "hh..........HH",
        "jh..........Hj",
        "hH..........HH",
        "jj..........jj",
    ]),
    "bun": (-1, -4, [
        "......hh......",
        ".....hjhH.....",
        "......hH......",
        "...hhhhhhhh...",
        "..hjjhhhhhhh..",
        ".hjhhhhhhhhhH.",
        ".hh.......hHH.",
        ".h..........H.",
        ".h..........H.",
    ]),
    "mane": (-2, -2, [
        "...hhhhhhhhhh...",
        "..hhjhhhhhhhhh..",
        ".hhjhhhhhhhhhhH.",
        "hhhhh......hhhHH",
        "hhh..........hHH",
        "hh............HH",
        "hh............HH",
        "hhh..........HHH",
        "hhh..........HHH",
        ".hhh........HHH.",
        "..hh........HH..",
    ]),
    "manelong": (-2, -2, [
        "...hhhhhhhhhh...",
        "..hhjhhhhhhhhh..",
        ".hhjhhhhhhhhhhH.",
        "hhhhh......hhhHH",
        "hhh..........hHH",
        "hh............HH",
        "hh............HH",
        "hhh..........HHH",
        "hhh..........HHH",
        "hhhh........HHHH",
        "hhh..........HHH",
        "hhh..........HHH",
        ".hh..........HH.",
        ".h............H.",
    ]),
    "wild": (-2, -2, [
        "....h.hhh.h.....",
        "..hhjhhjhhhhh...",
        ".hjhhhhhhjhhhH..",
        "hhjhhhhhhhhhjhH.",
        "hhh.hhh.h.hhhHH.",
        "hh...........HH.",
        "hh...........HHh",
        "hh............Hh",
        "hh............HH",
        "hhh..........HHH",
        ".hh..........HH.",
    ]),
    "wildlong": (-2, -2, [
        "....h.hhh.h.....",
        "..hhjhhjhhhhh...",
        ".hjhhhhhhjhhhH..",
        "hhjhhhhhhhhhjhH.",
        "hhh.hhh.h.hhhHH.",
        "hh...........HH.",
        "hh...........HHh",
        "hh............Hh",
        "hh............HH",
        "hhh..........HHH",
        "hhh..........HHH",
        "hhhh........HHHH",
        "hhh..........HHH",
        "hh............HH",
        ".h............H.",
        "h..............H",
    ]),
    "wisps": (-1, -1, [
        "....h.hh.h....",
        "..hjhhjhhhh...",
        ".hj.hh.hh.hH..",
        "h.h.......h.H.",
        "h............H",
        ".h..........H.",
        "h............H",
        ".h..........H.",
        "h............H",
        "..............",
        ".h..........H.",
    ]),
}

# The styles long enough to show below a helm or inside a hood.
LONG = {"long", "braids", "wisps", "curlylong", "stringy", "manelong",
        "wildlong"}


# --- what a race adds to a head ------------------------------------------------

EARS = {
    "round": (0, 4, [
        "S..........S",
        "s..........s",
        "S..........S",
    ]),
    # A Half-Elf's: round, with the beginning of a point.
    "half": (0, 3, [
        "s..........s",
        "S..........S",
        "s..........s",
        "S..........S",
    ]),
    "point": (-1, 2, [
        "s............s",
        ".s..........s.",
        ".s..........s.",
        ".S..........S.",
    ]),
    # A High-Elf's and a Sprite's: long, and out rather than up.
    "long": (-3, 2, [
        "ss..............ss",
        ".ss............ss.",
        "..ss..........ss..",
        "...S..........S...",
    ]),
    # A Yeek's hang down.
    "droop": (-2, 4, [
        "sss..........sss",
        "ssS..........Sss",
        "sS............Ss",
        "sS............Ss",
        "s..............s",
        "S..............S",
    ]),
    # A Kobold's stand up, as a dog's do.
    "dog": (0, -3, [
        ".s........s.",
        ".ss......sS.",
        ".sSs....sSS.",
    ]),
}

TUSKS = (0, 8, [
    "....T..T....",
    "....T..T....",
])

FANGS = (0, 10, [
    ".....T......",
])

TEETH = (3, 9, [
    "tmtmt",
])

BEARD = (-1, 6, [
    "..............",
    "..h........h..",
    ".hh........hH.",
    ".hhhh..hhhhhH.",
    ".hhhhhhhhhhhH.",
    "..hjhhhhhhHH..",
    "..hhjhhhhhHH..",
    "...hhhjhhHH...",
    "...hhhhhHH....",
    "....hhhHH.....",
    ".....hhH......",
])

# A Dunadan's, kept short.
STUBBLE = (0, 7, [
    ".h........h.",
    ".hh......hh.",
    ".hh......hh.",
    "..hhhhhhhh..",
    "...hhhhhh...",
])

# A Gnome's: moustache and a point of beard.
GOATEE = (2, 8, [
    ".hhhhhh.",
    "hh....hh",
    ".hhhhhh.",
    "..hhhh..",
    "...hh...",
])

NOSE = (4, 7, [
    "kss",
    "SSS",
])

STITCHES = (2, 1, [
    "z.z.z",
    "zzzzz",
    "z.z.z",
])

PAINT = (1, 6, [
    "vvvv..vvvv",
])

TENTACLES = (1, 9, [
    ".t.t..t.t.",
    ".t.t..t.t.",
    "t..tt.t..t",
    "t..T..T..T",
    ".t.t..t.t.",
    ".T.t..t.T.",
    "..t....t..",
    "..T....T..",
])

HORNS = (-2, -3, [
    "R..............R",
    "RR............RR",
    ".RR..........RR.",
    "..RR........RR..",
])

RAM_HORNS = (-2, 1, [
    "RRR..........RRR",
    "R..R........R..R",
    "Q..R........R..Q",
    ".RQ..........QR.",
])

CREST = (0, -2, [
    "...R.RR.R...",
    "...RRRRRR...",
])

ANTENNAE = (0, -4, [
    ".A........A.",
    "..A......A..",
    "...A....A...",
    "....A..A....",
])

# Wings, behind the body, from the canvas's left edge at the head's height.
WINGS = (0, 0, [
    "xx..........................xx",
    "xxx........................xxx",
    "xXxx......................xxXx",
    "xXXxx....................xxXXx",
    ".xXXxx..................xxXXx.",
    ".xxXXx..................xXXxx.",
    "..xxXxx................xxXxx..",
    "...xxxx................xxxx...",
    "..xXxx..................xxXx..",
    ".xXXx....................xXXx.",
    ".xXx......................xXx.",
    "..xx......................xx..",
])

# A Draconian's, a bat's: spars and a membrane, rising from the shoulders.
DRAGON_WINGS = (0, 1, [
    "X............................X",
    "XX..........................XX",
    "XxX........................XxX",
    "XxxX......................XxxX",
    "XxxxX....................XxxxX",
    "XxXxxX..................XxxXxX",
    ".XxXxxX................XxxXxX.",
    "..XxXxxX..............XxxXxX..",
    "...XxXxx..............xxXxX...",
    "....XxXx..............xXxX....",
    ".....X.x..............x.X.....",
])


# --- what a class wears and carries --------------------------------------------

HELM = (-1, -2, [
    "....pppppp....",
    "..pqqppppppP..",
    ".pqqpppppppPP.",
    ".pqppppppppPP.",
    ".pddddddddddP.",
    ".pp...pp...PP.",
    ".pp...pp...PP.",
    ".pP...pp...PP.",
    "..P........P..",
])

HELM_HORNS = (-3, -4, [
    "q..............q",
    "qq............qq",
    ".qq..........qq.",
    "..qq........qq..",
])

WIZARD_HAT = (-3, -5, [
    "...........cC.....",
    "..........ccC.....",
    ".........cccC.....",
    "........ccccCC....",
    ".......cccccCC....",
    "......cddddddDD...",
    "..cccccccccccccC..",
    ".CCCCCCCCCCCCCCCC.",
])

HOOD = (-2, -2, [
    ".....oooooo.....",
    "...oooooooooO...",
    "..ooooooooooOO..",
    ".ooSSSSSSSSSSOO.",
    ".oo..........OO.",
    ".oo..........OO.",
    ".oo..........OO.",
    ".oo..........OO.",
    ".oo..........OO.",
    ".oo..........OO.",
    ".ooo........OOO.",
    "..ooo......OOO..",
    "...oooooooooO...",
])

# Long hair inside a hood: a fringe, the strands either side of the face, and
# the ends on the chest below it.
HOOD_SPILL = (0, 2, [
    ".hhjh..hhhH.",
    ".h........H.",
    ".h........H.",
    ".j........H.",
    ".h........H.",
    ".h........j.",
    ".h........H.",
    ".j........H.",
    "............",
    "hh........HH",
    "hj........jH",
    "jh........Hj",
])

CIRCLET = (0, 2, [
    ".d..d..d..d.",
    "dddddddddddd",
])

GEM_CIRCLET = (0, 1, [
    ".....gg.....",
    "qqqqqgGqqqqq",
])

HEADBAND = (-1, 2, [
    "rrrrrrrrrrrrrr",
    "............ry",
    ".............y",
])

SWORD = (26, 3, ["..qW.."] + ["..WV.."] * 15 + [
    "dddddD",
    "..NN..",
    "..NN..",
    "..NN..",
    "..dD..",
])

STAFF = (25, 1, [
    "..gg..",
    ".gqgG.",
    ".ggGG.",
    "..GG..",
] + ["..nN.."] * 26)

CRYSTAL_STAFF = (25, 0, [
    "..g...",
    ".gqG..",
    ".gGG..",
    "gqgGG.",
    ".gGG..",
    "..G...",
] + ["..nN.."] * 26)

LEAF_STAFF = (24, 1, [
    ".dd.dd.",
    "ddDdDdd",
    ".dDnDd.",
    "..dnN..",
] + ["...nN.."] * 26)

SKULL_STAFF = (24, 0, [
    "..uuu..",
    ".uuuuU.",
    ".uKuKU.",
    "..uuU..",
    "..uUu..",
] + ["...nN.."] * 27)

AXE = (22, 2, [
    "......N..",
    "..WW..N..",
    ".WqWW.N..",
    "WqWWWVNV.",
    "WWWWWVNV.",
    "WWWWVVNV.",
    ".WWVV.N..",
    "..VV..N..",
] + ["......N.."] * 22)

DAGGER = (25, 20, [
    "..q..",
    ".qWV.",
    ".WWV.",
    ".WWV.",
    "ddddD",
    "..N..",
    "..N..",
    "..d..",
])

BOW = (1, 4, [
    "..n.",
    ".nw.",
] + ["nN.w"] * 17 + [
    ".nw.",
    "..n.",
])

QUIVER = (22, 12, [
    "f.F.f",
    "fFfFf",
    ".nNN.",
    ".nNN.",
    ".nNN.",
])

# A Monk's weapon is its hands: knuckles, the thumb across them, the
# wrapping, and an edge of its own, since it is held in front of the gi.
FIST = (19, 19, [
    "..eeeeee..",
    ".esksksse.",
    "esSsSsSsSe",
    "essSsSsSSe",
    "eskkkkkSSe",
    ".esSSSSSe.",
    ".euuuuuue.",
    ".euUuUuUe.",
    ".euuuuuue.",
    "..euuuue..",
    "..euuuue..",
])

# A High-Mage's collar, standing up behind the head.
COLLAR = (7, 0, [
    "d................d",
    "cd..............dc",
    "ccd............dcc",
    "Cccd..........dccC",
    "CCcc..........ccCC",
    ".CCc..........cCC.",
])


# --- races ---------------------------------------------------------------------
#
# build      (height, width): S, M or L.  Height places the head, width the
#            shoulders.
# head       a head above, or {sex: head}; a woman's round or square head is
#            the softer roundf unless the race says otherwise.
# hair       {sex: style}; a sex with none is bald.
# features   what else it has, drawn in this order: ears, nose, tusks, fangs,
#            teeth, stitches, paint, tentacles, beard (by sex), stubble,
#            goatee -- and after any headgear, horns, ram horns and crest.
# back       wings.
# palF       colours a woman has in place of the race's own.

def race(**k):
    return k


HUMANLIKE = dict(w="#f4f0e8", l="#b85a50", m="#8a4a3a")

RACES = {
    "Human": race(
        build=("M", "M"), head="round", ears="round",
        hair={"M": "short", "F": "long"},
        pal=dict(HUMANLIKE, s="#e0a878", S="#b87c54", k="#f0c49a",
                 h="#6a4428", H="#4a2e1a", j="#8a5c38", E="#3a5a9a")),
    "Half-Elf": race(
        build=("M", "M"), head="round", ears="half",
        hair={"M": "short", "F": "long"},
        pal=dict(HUMANLIKE, s="#ecc09a", S="#c09670", k="#f8d6b4",
                 h="#9a6a3a", H="#6a4422", j="#c08a50", E="#6a7a3a")),
    "Elf": race(
        build=("M", "M"), head="round", ears="point",
        hair={"M": "short", "F": "long"},
        pal=dict(s="#f2d4b4", S="#c8a684", k="#fbe6cc", h="#e8c858",
                 H="#b89432", j="#fff0a0", E="#3a8a4a", w="#f8f4ec",
                 l="#c86a70", m="#a06050")),
    "Hobbit": race(
        build=("S", "S"), head="round", ears="point",
        hair={"M": "curly", "F": "curlylong"},
        pal=dict(HUMANLIKE, s="#eab48a", S="#c08660", k="#f6cca8",
                 h="#8a5a2a", H="#5e3a18", j="#b8843e", E="#5a3a1a")),
    "Gnome": race(
        build=("S", "S"), head="round", ears="point", nose=True,
        goatee={"M"}, hair={"M": "short", "F": "bun"},
        pal=dict(s="#f0b898", S="#c88a6c", k="#fcd2b8", h="#e8e4e0",
                 H="#b0aaa4", j="#ffffff", E="#4a6aa0", w="#f8f4ee",
                 l="#c87068", m="#8a4a3a")),
    "Dwarf": race(
        build=("S", "L"), head="round", ears="round",
        hair={"M": "short", "F": "braids"}, beard={"M"},
        pal=dict(s="#d89070", S="#a86448", k="#eeae8c", h="#a8481e",
                 H="#702e10", j="#d07034", E="#3a4a6a", w="#f0e8e0",
                 l="#b04a40", m="#7a3a2a")),
    "Half-Orc": race(
        build=("M", "L"), head="square", ears="point", tusks=True,
        hair={"M": "short", "F": "long"},
        pal=dict(s="#8aa070", S="#627a4c", k="#a8bc8a", h="#2a2626",
                 H="#141212", j="#4a4444", E="#c03020", w="#e8e4c8",
                 l="#6a5040", m="#3a2a1a", T="#f4f0dc")),
    "Half-Troll": race(
        build=("L", "L"), head={"M": "brute", "F": "brute"}, ears="point",
        hair={"M": "tuft", "F": "stringy"},
        pal=dict(s="#6e8a5a", S="#4a6238", k="#8aa874", h="#2a2a22",
                 H="#14140e", j="#4a4a3a", E="#f0d020", w="#f0d020",
                 l="#4a3a2a", m="#2a1a10", T="#e8e4cc", z="#3a4a2a",
                 b="#4a6238")),
    "Dunadan": race(
        build=("M", "M"), head="round", ears="round", stubble={"M"},
        hair={"M": "short", "F": "long"},
        pal=dict(HUMANLIKE, s="#e8bc94", S="#bc906a", k="#f6d4b2",
                 h="#2a2018", H="#14100a", j="#4a3a2c", E="#8a9aaa")),
    "High-Elf": race(
        build=("M", "M"), head="round", ears="long",
        hair={"M": "long", "F": "long"},
        pal=dict(s="#f8e4d4", S="#d4bca8", k="#fff4ea", h="#e8e8f4",
                 H="#b0b4c8", j="#ffffff", E="#70d8ff", w="#d8f4ff",
                 l="#d08a90", m="#a06a6a")),
    "Kobold": race(
        build=("S", "S"), head="snout", ears="dog", hair={},
        pal=dict(s="#a8603a", S="#7a3e20", k="#c8804e", u="#d8a070",
                 U="#b07a50", z="#1a0e0a", E="#1a0e0a", w="#f8f0e0",
                 m="#4a2010", b="#7a3e20"),
        palF=dict(s="#b87a46", S="#8a5228", k="#d89a64", b="#8a5228")),
    "Amberite": race(
        build=("M", "M"), head="round", ears="round", jewel=True,
        hair={"M": "short", "F": "long"},
        pal=dict(s="#ecc8a8", S="#c09c7c", k="#f8dcc0", h="#181418",
                 H="#0a080a", j="#3a3238", E="#30a050", w="#f6f4ee",
                 l="#b05a5a", m="#7a3a3a", J="#d02020", Z="#e8c040")),
    "Beastman": race(
        build=("M", "L"), head="snout", ears="point", ram_horns=True,
        hair={"M": "mane", "F": "manelong"},
        pal=dict(s="#8a7058", S="#5e4a38", k="#a88a70", u="#b49a7a",
                 U="#8a7258", z="#1a1410", E="#e0b020", w="#e0b020",
                 h="#4a3a2a", H="#2a2018", j="#6a5440", m="#2a1a10",
                 b="#5e4a38", R="#d8ccb0", Q="#a89a80")),
    "Yeek": race(
        build=("S", "S"), head="round", ears="droop",
        hair={"F": "short"},
        pal=dict(s="#8aa0c8", S="#6078a0", k="#a8bce0", E="#101018",
                 w="#ffffff", h="#4a5a7a", H="#2a3a5a", j="#6a7a9a",
                 l="#6a5a8a", m="#3a3a5a", b="#6078a0")),
    "Draconian": race(
        build=("M", "L"), head="snout", crest=True, back="dragon", hair={},
        pal=dict(s="#4a8a4a", S="#2e6030", k="#7ac07a", u="#b0c870",
                 U="#8aa050", z="#1a2a10", E="#ffd020", w="#ffd020",
                 m="#1a2a10", b="#2e6030", R="#d8d0a0", x="#3a7a3a",
                 X="#1a3a1a"),
        palF=dict(s="#9a7a3a", S="#6a5222", k="#c8a460", b="#6a5222",
                  x="#8a6a2e", X="#3a2a10")),
    "Mindflayer": race(
        build=("M", "M"), head={"M": "bulb", "F": "bulb"}, tentacles=True,
        hair={},
        pal=dict(s="#9a6aa8", S="#6a447a", k="#b88ac4", w="#f0f0f8",
                 t="#8a5a98", T="#5a3468", m="#3a2040"),
        palF=dict(s="#b06aa0", S="#7a4470", k="#d08ac0", t="#a05a90",
                  T="#6a3460")),
    "Vampire": race(
        build=("M", "M"), head="round", ears="point", fangs=True,
        hair={"M": "slick", "F": "long"},
        pal=dict(s="#e8e0e8", S="#b8acbc", k="#fcf8fc", h="#141018",
                 H="#08060a", j="#3a3040", E="#e01818", w="#ffe0e0",
                 l="#8a1a2a", m="#5a1018", T="#ffffff")),
    "Golem": race(
        build=("L", "L"), head={"M": "block", "F": "block"}, hair={},
        same_body=True,
        pal=dict(s="#8a8478", S="#5e5a52", k="#aca698", z="#3a3832",
                 g="#ffa030", G="#c06010", m="#3a3832"),
        palF=dict(s="#b07a58", S="#7e5236", k="#cc9a78", z="#5a3420",
                  m="#5a3420")),
    "Barbarian": race(
        build=("M", "L"), head="square", ears="round", paint=True,
        hair={"M": "wild", "F": "wildlong"},
        pal=dict(s="#d8a070", S="#a87448", k="#ecbc90", h="#c8742a",
                 H="#8a4a14", j="#e89a4a", E="#3a6aa0", w="#f4eee4",
                 l="#b0605a", m="#7a3a2a", v="#3a5ad0")),
    "Klackon": race(
        build=("M", "M"), head={"M": "insect", "F": "insect"},
        antennae=True, hair={}, same_body=True,
        pal=dict(a="#b04a2a", A="#742812", K="#e88050", E="#141010",
                 w="#8a8a96", M="#3a1a0a"),
        palF=dict(a="#c0682a", A="#844014", K="#f09a50")),
    "Nibelung": race(
        build=("S", "L"), head="round", ears="round",
        hair={"M": "short", "F": "braids"}, beard={"M"},
        pal=dict(s="#8a8a9a", S="#5e5e70", k="#a8a8b8", h="#202028",
                 H="#101016", j="#3a3a48", E="#c0c8ff", w="#e0e4ff",
                 l="#6a5a6a", m="#3a2a3a")),
    "Imp": race(
        build=("S", "S"), head="round", ears="point", horns=True,
        hair={"M": "short", "F": "long"}, fangs=True,
        pal=dict(s="#c4402c", S="#8a2418", k="#e8704c", h="#1e1010",
                 H="#0c0606", j="#3e2a2a", E="#ffd420", w="#ffd420",
                 l="#7a1a10", m="#4a0c06", R="#5a4430", T="#f8f0e0")),
    "Skeleton": race(
        build=("M", "M"), head={"M": "skull", "F": "skull"},
        hair={"F": "stringy"},
        pal=dict(s="#e8e0cc", S="#a89c84", k="#fffaf0", z="#1a1414",
                 E="#e04020", t="#f8f4e8", m="#1a1414", h="#8a8478",
                 H="#5e5a52", j="#aca698")),
    "Zombie": race(
        build=("M", "M"), head="round", ears="round", stitches=True,
        hair={"M": "messy", "F": "stringy"},
        pal=dict(s="#8a9a7a", S="#5a6a4a", k="#a8b894", h="#4a4030",
                 H="#2a2418", j="#6a5a44", E="#d8d8b0", w="#c8c8a0",
                 l="#4a5a3a", m="#2a3020", z="#6a1a1a")),
    "Spectre": race(
        build=("M", "M"), head="round", ears=None,
        hair={"M": "wisps", "F": "wisps"}, ghost=True,
        pal=dict(s="#c8dcf0", S="#8ea6c6", k="#eef6ff", h="#e4f0fa",
                 H="#a0b8d0", j="#ffffff", E="#50f0ff", w="#50f0ff",
                 l="#90a8c8", m="#5878a0", b="#a0b8d0"),
        outline="#2a6a8a"),
    "Ghoul": race(
        build=("M", "M"), head="round", ears="point", teeth=True,
        hair={"F": "stringy"},
        pal=dict(s="#9aa4a8", S="#6a747a", k="#bcc4c8", E="#f0f0e0",
                 w="#f0f0e0", t="#f4f0e0", m="#1a1414", h="#5a6064",
                 H="#3a4044", j="#7a8084", l="#5a6064")),
    "Sprite": race(
        build=("S", "S"), head="round", ears="long", back="wings",
        hair={"M": "short", "F": "long"},
        pal=dict(s="#f8dcd0", S="#d6b0a4", k="#fff2ec", h="#e070c0",
                 H="#a84890", j="#ffb0ec", E="#30b090", w="#ffffff",
                 l="#e07090", m="#b05a6a", x=("#a8ecff", 150),
                 X=("#e8fcff", 190))),
    "Half-Titan": race(
        build=("L", "L"), head="square", ears="round",
        hair={"M": "short", "F": "long"},
        pal=dict(s="#c89c5c", S="#96703c", k="#e8c27e", h="#f0c040",
                 H="#b88a20", j="#fff080", E="#f0b020", w="#fff8e0",
                 l="#a0603a", m="#6a4020")),
}


# --- classes -------------------------------------------------------------------
#
# outfit     how the torso is dressed: plate, spiked, chaos, tabard, halfplate,
#            robe, leather or gi.
# head       headgear; hides_hair if the race's hair goes under it, and spill
#            for what of a long style shows anyway (below a helm, by default).
# horns      the headgear's own, left off for a race with horns of its own.
# item       carried in front; back for what is carried behind.
# aura, stars, collar, cloak, symbol    the rest of what marks the class.
# backdrop   the portrait's ground, top and bottom.

def klass(**k):
    return k


def plate(c="#8a94a6", C="#5c6475", d="#c8a24a", D="#8a6a28",
          p="#b4bccb", P="#6e7686", q="#eef2f8"):
    return dict(c=c, C=C, d=d, D=D, p=p, P=P, q=q)


CLASSES = {
    "Warrior": klass(
        outfit="plate", head=HELM, hides_hair=True, item=SWORD,
        pal=dict(plate(), W="#e8eef4", V="#9aa4b4", N="#5a3a22"),
        backdrop=("#4a5466", "#1c2230")),
    "Mage": klass(
        outfit="robe", head=WIZARD_HAT, item=STAFF,
        pal=dict(c="#3456b4", C="#20387a", d="#e8b840", D="#a87e20",
                 g="#80f0ff", G="#30a8d8", q="#ffffff", n="#8a5a30",
                 N="#5a3818", i="#e8b840"),
        backdrop=("#26305e", "#0e1228")),
    "Druid": klass(
        outfit="robe", head=CIRCLET, item=LEAF_STAFF,
        pal=dict(c="#6a5a2e", C="#44381a", d="#5ab040", D="#2e7a24",
                 n="#9a6a34", N="#5e3e1a", i="#5ab040"),
        backdrop=("#3a4a1e", "#121a08")),
    "Priest": klass(
        outfit="robe", symbol="cross",
        pal=dict(c="#ece4cc", C="#b8ae94", d="#e0b030", D="#a07818",
                 i="#e0b030"),
        backdrop=("#6a5426", "#2a200c")),
    "Necromancer": klass(
        outfit="robe", head=HOOD, spill=HOOD_SPILL, hides_hair=True,
        item=SKULL_STAFF,
        pal=dict(c="#2c1a3a", C="#160c20", d="#8a44c0", D="#5a2a80",
                 o="#2a1838", O="#140a1c", n="#4a3a2a", N="#2a2018",
                 u="#e8e0cc", U="#a89c84", K="#200810", i="#8a44c0"),
        backdrop=("#2e1440", "#0c0414")),
    "Paladin": klass(
        outfit="tabard", item=SWORD,
        pal=dict(plate(c="#c0c8d6", C="#7a8496", p="#d8dee8", P="#8a94a6"),
                 i="#f4f0e4", r="#c02828", W="#f4f8fc", V="#a8b2c2",
                 N="#5a3a22"),
        backdrop=("#2a3e78", "#0c1430")),
    "Rogue": klass(
        outfit="leather", head=HOOD, spill=HOOD_SPILL, hides_hair=True,
        item=DAGGER,
        pal=dict(c="#4a3a2a", C="#2c2218", d="#7a5a3a", D="#4a3420",
                 o="#5a6650", O="#3a4434", W="#dce4ec", V="#8a94a0",
                 q="#ffffff", N="#3a2414"),
        backdrop=("#323a4c", "#0e1018")),
    "Ranger": klass(
        outfit="leather", back=[BOW, QUIVER], cloak=True,
        pal=dict(c="#3a6a2a", C="#24461a", d="#7a5a3a", D="#4e3820",
                 n="#9a6a34", N="#5e3e1a", w="#e8e0c8", f="#c83a2a",
                 F="#f0ece0"),
        backdrop=("#2e4a26", "#0e1a0a")),
    "Blackguard": klass(
        outfit="spiked", item=AXE,
        pal=dict(plate(c="#2e2e38", C="#18181e", d="#a82020", D="#6a1010",
                       p="#5a5a68", P="#34343e", q="#9a9aa8"),
                 N="#6a4a2a", W="#dce2ea", V="#8a92a0"),
        backdrop=("#4a1414", "#140404")),
    "Monk": klass(
        outfit="gi", head=HEADBAND, item=FIST,
        pal=dict(c="#d07a20", C="#8e4c10", d="#2a1a10", D="#140c06",
                 u="#ece6d8", U="#b4ac98", r="#c02a2a", y="#7a1414",
                 i="#f0d8b0", e="#24140a"),
        backdrop=("#5a3a1a", "#1a0e04")),
    "Mindcrafter": klass(
        outfit="robe", head=GEM_CIRCLET, aura="#ff90f0",
        pal=dict(c="#7a3a8a", C="#4a1e56", d="#c8d0dc", D="#8a94a4",
                 q="#e8eef6", g="#ff70e0", G="#c030a0", i="#c8d0dc"),
        backdrop=("#30204a", "#0c0618")),
    "Chaos-Warrior": klass(
        outfit="chaos", head=HELM, horns=HELM_HORNS, hides_hair=True,
        item=SWORD,
        pal=dict(plate(c="#5a2a26", C="#361614", d="#f07a20", D="#a84a10",
                       p="#6e3a34", P="#42201c", q="#e8d8c0"),
                 W="#ffb070", V="#c04020", N="#2a1410"),
        backdrop=("#5a2410", "#1a0804")),
    "Warrior-Mage": klass(
        outfit="halfplate", item=SWORD,
        pal=dict(plate(c="#2a5a7a", C="#1a3a52", d="#d8b040", D="#9a7a20"),
                 W="#a8e4ff", V="#4aa0d8", N="#3a2a1a", i="#d8b040"),
        backdrop=("#24405a", "#0a1420")),
    "High-Mage": klass(
        outfit="robe", stars=True, collar=True, item=CRYSTAL_STAFF,
        pal=dict(c="#4a2a7a", C="#2a1648", d="#d4d8e8", D="#9098b0",
                 q="#ffffff", g="#d8a8ff", G="#7a40c0", n="#3a2a1a",
                 N="#1e140c", i="#d4d8e8"),
        backdrop=("#2a1648", "#0a0418")),
}
