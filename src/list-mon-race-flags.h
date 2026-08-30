/**
 * \file list-mon-race-flags.h
 * \brief monster race flags
 *
 */
/* symbol		type			descr */
RF(NONE,		RFT_NONE,		"")
RF(UNIQUE,		RFT_OBV,		"")
RF(QUESTOR,		RFT_OBV,		"")
RF(MALE,		RFT_OBV,		"")
RF(FEMALE,		RFT_OBV,		"")
RF(GROUP_AI,	RFT_OBV,		"")
RF(NAME_COMMA,	RFT_OBV,		"")
RF(CHAR_CLEAR,	RFT_DISP,		"")
RF(ATTR_RAND,	RFT_DISP,		"")
RF(ATTR_CLEAR,	RFT_DISP,		"")
RF(ATTR_MULTI,	RFT_DISP,		"")
RF(ATTR_FLICKER,RFT_DISP,		"")
RF(FORCE_DEPTH,	RFT_GEN,		"")
RF(FORCE_SLEEP,	RFT_GEN,		"")
RF(FORCE_EXTRA,	RFT_GEN,		"")
RF(SEASONAL,	RFT_GEN,		"")
RF(UNAWARE,		RFT_NOTE,		"")
RF(MULTIPLY,	RFT_NOTE,		"")
RF(REGENERATE,	RFT_NOTE,		"")
RF(BLESSING,	RFT_NOTE,		"")
RF(FRIGHTENED,	RFT_BEHAV,		"")
RF(NEVER_BLOW,	RFT_BEHAV,		"")
RF(NEVER_MOVE,	RFT_BEHAV,		"")
RF(RAND_25,		RFT_BEHAV,		"")
RF(RAND_50,		RFT_BEHAV,		"")
RF(MIMIC_INV,	RFT_BEHAV,		"")
RF(STUPID,		RFT_BEHAV,		"")
RF(SMART,		RFT_BEHAV,		"")
RF(SPIRIT,		RFT_BEHAV,		"")
RF(POWERFUL,	RFT_BEHAV,		"")
RF(ONLY_GOLD,	RFT_DROP,		"")
RF(ONLY_ITEM,	RFT_DROP,		"")
RF(DROP_40,		RFT_DROP,		"")
RF(DROP_60,		RFT_DROP,		"")
RF(DROP_1,		RFT_DROP,		"")
RF(DROP_2,		RFT_DROP,		"")
RF(DROP_3,		RFT_DROP,		"")
RF(DROP_4,		RFT_DROP,		"")
RF(DROP_GOOD,	RFT_DROP,		"")
RF(DROP_GREAT,	RFT_DROP,		"")
RF(DROP_20,		RFT_DROP,		"")
RF(INVISIBLE,	RFT_DET,		"invisible")
RF(COLD_BLOOD,	RFT_DET,		"cold blooded")
RF(EMPTY_MIND,	RFT_DET,		"not detected by telepathy")
RF(WEIRD_MIND,	RFT_DET,		"rarely detected by telepathy")
RF(OPEN_DOOR,	RFT_ALTER,		"open doors")
RF(BASH_DOOR,	RFT_ALTER,		"bash down doors")
RF(PASS_WALL,	RFT_ALTER,		"pass through walls")
RF(KILL_WALL,	RFT_ALTER,		"bore through walls")
RF(SMASH_WALL,	RFT_ALTER,		"smash walls")
RF(MOVE_BODY,	RFT_ALTER,		"push past weaker monsters")
RF(KILL_BODY,	RFT_ALTER,		"destroy weaker monsters")
RF(TAKE_ITEM,	RFT_ALTER,		"pick up objects")
RF(KILL_ITEM,	RFT_ALTER,		"destroy objects")
RF(CLEAR_WEB,	RFT_ALTER,		"clear webs")
RF(PASS_WEB,	RFT_ALTER,		"pass through webs")
RF(ORC,			RFT_RACE_N,		"orc")
RF(TROLL,		RFT_RACE_N,		"troll")
RF(GIANT,		RFT_RACE_N,		"giant")
RF(DRAGON,		RFT_RACE_N,		"dragon")
RF(DEMON,		RFT_RACE_N,		"demon")
RF(ANIMAL,		RFT_RACE_A,		"natural")
RF(EVIL,		RFT_RACE_A,		"evil")
RF(UNDEAD,		RFT_RACE_A,		"undead")
RF(NONLIVING,	RFT_RACE_A,		"nonliving")
RF(METAL,		RFT_RACE_A,		"metal")
RF(HURT_LIGHT,	RFT_VULN,		"bright light")
RF(HURT_ROCK,	RFT_VULN,		"rock remover")
RF(HURT_FIRE,	RFT_VULN_I,		"fire")
RF(HURT_COLD,	RFT_VULN_I,		"cold")
RF(IM_ACID,		RFT_RES,		"acid")
RF(IM_ELEC,		RFT_RES,		"lightning")
RF(IM_FIRE,		RFT_RES,		"fire")
RF(IM_COLD,		RFT_RES,		"cold")
RF(IM_POIS,		RFT_RES,		"poison")
RF(IM_NETHER,	RFT_RES,		"nether")
RF(IM_WATER,	RFT_RES,		"water")
RF(IM_PLASMA,	RFT_RES,		"plasma")
RF(IM_NEXUS,	RFT_RES,		"nexus")
RF(IM_DISEN,	RFT_RES,		"disenchantment")
RF(NO_FEAR,		RFT_PROT,		"frightened")
RF(NO_STUN,		RFT_PROT,		"stunned")
RF(NO_CONF,		RFT_PROT,		"confused")
RF(NO_SLEEP,	RFT_PROT,		"slept")
RF(NO_HOLD,		RFT_PROT,		"held")
RF(NO_SLOW,		RFT_PROT,		"slowed")
/*
 * ZangbandTK (CNT-02).  A scion of Amber: the twelve of Oberon's blood, which
 * is a kind of creature here rather than a label.  Appended rather than filed
 * with the other RFT_RACE_N flags because a monster's known flags are stored by
 * position in the lore, and inserting one in the middle would shift every flag
 * after it in a savefile already written.
 */
RF(AMBERITE,	RFT_RACE_N,		"Amberite")
/*
 * ZangbandTK (CNT-04).  The monster has something to say -- in a fight, when it
 * turns to run, and when it dies.  No lore line, because Zangband gave it none:
 * you find out a thing can talk by hearing it.  Appended for the same savefile
 * reason as AMBERITE above.
 */
RF(CAN_SPEAK,	RFT_OBV,		"")
/*
 * ZangbandTK (CNT-04).  Hard to move by force.  RFT_PROT with the other
 * immunities, since that is what it reads as in the lore -- "cannot be
 * teleported" beside "cannot be slept".  Appended, like the two above, because
 * lore stores known flags by position.
 */
RF(RES_TELE,	RFT_PROT,		"teleported")
/*
 * ZangbandTK (CNT-04).  Bolts bounce off it, and it burns, freezes or shocks
 * whoever touches it.  RFT_NOTE because the lore describes them in prose --
 * "surrounded by flames" -- rather than in a list of one-word properties.
 * Appended, like the three above, because lore stores flags by position.
 */
RF(REFLECTING,	RFT_NOTE,		"")
RF(AURA_FIRE,	RFT_NOTE,		"")
RF(AURA_COLD,	RFT_NOTE,		"")
RF(AURA_ELEC,	RFT_NOTE,		"")
/*
 * ZangbandTK (CNT-04).  Shows as something else, and something else again the
 * next time you look.  RFT_DISP with the other display flags, because that is
 * all it is -- the monster does not change, only the glyph does.  Appended, for
 * the same savefile reason as the flags above.
 */
RF(SHAPECHANGER,RFT_DISP,		"")
/*
 * ZangbandTK (CNT-04).  A creature that is only doubtfully there: half of what
 * is swung at it passes through, it moves only half the time, and it may simply
 * stop existing.  RFT_NOTE because the lore says it in prose.
 */
RF(QUANTUM,	RFT_NOTE,		"")
/* end flags */
