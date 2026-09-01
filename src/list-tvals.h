/**
 * \file list-tvals.h
 * \brief List of object base types
 */
/* code_name, string_name */
TV(NULL, "none")
TV(CHEST, "chest")
TV(SHOT, "shot")
TV(ARROW, "arrow")
TV(BOLT, "bolt")
TV(BOW, "bow")
TV(DIGGING, "digger")
TV(HAFTED, "hafted")
TV(POLEARM, "polearm")
TV(SWORD, "sword")
TV(BOOTS, "boots")
TV(GLOVES, "gloves")
TV(HELM, "helm")
TV(CROWN, "crown")
TV(SHIELD, "shield")
TV(CLOAK, "cloak")
TV(SOFT_ARMOR, "soft armor")
TV(HARD_ARMOR, "hard armor")
TV(DRAG_ARMOR, "dragon armor")
TV(LIGHT, "light")
TV(AMULET, "amulet")
TV(RING, "ring")
TV(STAFF, "staff")
TV(WAND, "wand")
TV(ROD, "rod")
TV(SCROLL, "scroll")
TV(POTION, "potion")
TV(FLASK, "flask")
TV(FOOD, "food")
TV(MUSHROOM, "mushroom")
TV(MAGIC_BOOK, "magic book")
TV(PRAYER_BOOK, "prayer book")
TV(NATURE_BOOK, "nature book")
TV(SHADOW_BOOK, "shadow book")
TV(OTHER_BOOK, "other book")
/*
 * ZangbandTK (CNT-10): a book for each realm 4.2 had no counterpart for.
 *
 * Safe to add here rather than only at the end: an object records its base by
 * *name* (`tval_find_name()` in save.c), not by this list's numbering, so the
 * order is an in-memory detail. The nouns are realm.txt's own book-nouns, so
 * that "You have a Sorcery Book" and the realm's own description agree.
 */
TV(SORCERY_BOOK, "sorcery book")
TV(CHAOS_BOOK, "chaos book")
TV(DECK, "deck")
TV(GOLD, "gold")
