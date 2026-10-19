# The Naughty List

Distilled from Wikipedia's [Signs of AI writing](https://en.wikipedia.org/wiki/Wikipedia:Signs_of_AI_writing)
(WikiProject AI Cleanup, as of October 2026), cut down to what applies to a
game manual, and tested against this repository's own pages. The rule
numbers match `scripts/slop_lint.py`. Rules with no lint check are judged
by reading.

Before the list, three warnings from the page itself:

- **The signs are symptoms.** The page warns that fixing
  the signs alone "could just make detection harder." The real problem is
  underneath: a sentence that only adds emphasis, a claim gone vague, a
  paragraph about the writing process instead of the game. If a sentence
  exists only to stage another sentence, delete it. Don't rephrase it.
- **Models regress to the mean.** They swap a specific, odd fact for a
  generic, impressive-sounding one, so the text gets louder and blurrier
  at once. The cure is the specific fact, said plainly.
- **One sign proves nothing.** Clusters do. People spot AI text at about
  chance level, and plenty of humans write some of these on purpose.
  Weak rules (marked *weak*) need company before you act on them.

How this manual measures up: the inherited Angband text, written by people
before November 2022, scores about **7** hits per 1000 words. The 2026 text
scored **37** before the first un-slop pass. The difference is almost all
N1, N8, N18, N21 and N31, plus the near-total absence of contractions.

---

## A. Staging: sentences that point at other sentences

These are the strongest signs in current model prose, and the most common
ones here. Act on a single sighting.

### N1. Not X but Y

**Watch for:** *not X but Y*, *not just/only/merely X but Y*, *it's not X,
it's Y*, *X, not Y*, *Y rather than X* (the reversed form, now the most
common), *no X, no Y, just Z*, and the same contrast split over two
sentences.

**Why it's a sign:** the text sounds as if it's clearing up a misconception
nobody had. The negative half adds weight and no information.

**What to do:** state the positive half. Keep a contrast only when a reader
really would believe the negative half (an Angband player assumes Angband's
rules, so "unlike Angband, ..." is fine), or when both halves are facts the
reader needs.

> Before: The world holds a dozen places, not one.
>
> After: There are a dozen towns in the world.

> Before: Savefiles carry the tag `ZZK1`, and the game will decline to open
> anything else rather than load it partially.
>
> After: Savefiles are tagged `ZZK1`, and the game won't open anything else.
> It doesn't try to load part of one.

### N2. Denials nobody asked for

**Watch for:** *That is not a rule...*, *It is not a port*, *Those figures
are not invented*, *That is not a gap*, *X is not Y's alone*.

**Why:** an answer to an objection that appears nowhere in the text. It's
usually left over from the conversation the page was written in, where
someone did ask.

**What to do:** delete the denial and keep whatever fact it guarded. If the
fact is that something was measured, say where the measurement came from.

> Before: Those figures are not invented. They are what Zangband did,
> measured across the 450 monsters ...
>
> After: The figures come from Zangband itself, measured across the 450
> monsters ...

### N3. One-line closers, aphorisms and dramatic fragments

**Watch for:** a one-sentence paragraph that sums up the one before
("ZangbandTK has three sides."), a short line that lands a point ("Its
*character* is."), a row of fragments, *which is the point*.

**Why:** it asks the reader to pause on a claim instead of adding to it.

**What to do:** fold it into the paragraph it belongs to, or cut it. A short
sentence is fine when it carries a new fact.

### N4. Staged run-ups, and pages describing themselves

**Watch for:** *This chapter states plainly how...*, *This page is the honest
inventory*, *Three things, in the order you will notice them.*, *Two
consequences follow immediately, and both are deliberate:*, *it is worth
stating why*, *here it is plainly:*.

**Why:** it announces the point instead of making it. A page that describes
its own honesty or plainness is doing the reader's judging for them.

**What to do:** start with the first real fact. A one-line page intro that
says what's on the page is fine ("Licensing, history and credits."). An
intro that grades the page isn't.

### N5. Rule of three (*weak*)

**Watch for:** lists of three adjectives or short phrases, three parallel
examples, the same triad repeated on several pages ("the wilderness, the
bestiary, the lethality").

**Why:** threes sound complete whether or not the meaning has three parts.

**What to do:** keep three when there are three. Otherwise use the real
number of items, or the strongest one.

### N6. Summary closers

**Watch for:** *In short*, *Overall*, *That is the whole of it*, a closing
paragraph that restates the section.

**What to do:** end on the last concrete fact.

---

## B. Inflation: dressing up a fact

The fact underneath is usually right. Keep it and take the dressing off.

### N7. Puffed-up significance

**Watch for:** *testament*, *pivotal*, *crucial/vital/key role*,
*underscores*, *marks a shift*, *legacy*, *reflects a broader*, *setting the
stage*, *the distinction matters because*.

**What to do:** say what the thing does in the game.

### N8. Sincerity and intent flags (this repository's commonest word habit)

**Watch for:** *honest(ly)*, *plainly*, *simply*, *genuinely*, *actually*, *a
real X*, *on purpose*, *deliberately*, *by design*, *says so*, *worth* (*worth
stating*, *worth standing behind*, *worth doing on purpose*), *the point*,
*quietly*, *silently*, *the whole of*, *in whichever direction*.

**Why:** these tell the reader how to feel about the text: that it's
honest, intended, important. Wikipedia's mid-2026 word list has *quietly*
and *matters*, and this manual's 2026 text uses the rest hundreds of times.
The human text hardly uses them.

**What to do:** delete the flag. If the intent is a real fact ("the game
does this on purpose, because ..."), keep the *because* and drop the *on
purpose*. "Worth" stays when it's a real value judgement a player can use
("a stable worth having" is fine once; not on every page).

### N9. Brochure language

**Watch for:** *boasts*, *vibrant*, *rich*, *profound*, *showcasing*,
*seamless*, *stunning*, *immersive*, *in the heart of*, *a world of*.

**What to do:** say what's there.

### N10. -ing riders

**Watch for:** a fact followed by *, highlighting...*, *, ensuring...*, *,
reflecting...*, *, making it...*, *, creating a...*.

**What to do:** keep the fact. Keep the rider only if it's a second fact the
game supports, and then make it its own clause.

### N11. Vague connection and attribution

**Watch for:** *associated with*, *in connection with*, *linked to*, *many
players*, *experts*, *it is often said*, and *such as* before a list that's
actually complete.

**What to do:** name the relationship ("the shop sells", "the patron
grants"). If you don't know it, keep the vague wording. Never make one up.

### N12. Status brags and the challenges-and-outlook formula

**Watch for:** *feature-complete*, *the polish that turns a working game into
one worth standing behind*, *despite these challenges*, *the future looks
bright*.

**What to do:** say what's done and what isn't, as a list if it's long.
Status lines go stale (see the website memory), so don't polish them.
Check them against `git log` and report what's out of date.

---

## C. Words

### N13. AI vocabulary density

The words change with each model generation. The page's eras:

- **2023 to mid-2024:** additionally, boasts, bolstered, crucial, delve,
  emphasizing, enduring, garner, intricate, interplay, key, landscape,
  meticulously, pivotal, underscore, tapestry, testament, valuable, vibrant
- **mid-2024 to mid-2025:** align with, enhance, fostering, highlighting,
  showcasing (plus the above)
- **mid-2025 to mid-2026:** emphasizing, enhance, highlighting, showcasing
- **mid-2026 on:** challenge, clearer, dependable, echoed, foster, leverage,
  matters, multifaceted, practical, prioritize, quietly, steady, universally
- **Grok:** causal, empirical, correlate

Take the list literally: an overused word doesn't make its synonyms
suspect. One or two in a chapter is nothing. A cluster is the strongest
word-level sign there is. Literal uses are fine (the *key* on a keyboard,
*highlight* a menu item).

### N14. Avoiding *is* and *has*

**Watch for:** *serves as*, *stands as*, *acts as*, *functions as*,
*represents*, *offers*, *features*, *boasts*, *maintains*, *refers to*. In
this manual also *carries* for *has* ("every monster carries 73%").

**What to do:** use *is*, *are*, *has*. One study found *is* and *are*
dropped by over 10% in academic writing in 2023.

### N15. Stiff synonyms (*weak*)

*utilise*, *commence*, *facilitate*, *relocate*, *endeavour*, *prior to*,
*possess*. Use *use*, *start*, *help*, *move*, *try*, *before*, *have*.

### N16. Elegant variation (*weak*, older models)

Cycling synonyms to avoid repeating a word (*the creature*, *the beast*,
*the foe*). In a manual, call a thing by one name, the one the game uses.

---

## D. Register and punctuation

### N17. No contractions anywhere

**Why:** not on Wikipedia's list, but measured here. The inherited guide
uses 57 contractions to 29 spelled-out forms. The four 2026 chapters
measured have 1 contraction to about 380 spelled-out forms. Uniform formal
register is a model default, and it's the main reason the new pages read
stiff next to the old ones.

**What to do:** use contractions where you'd say them aloud: *don't*,
*can't*, *won't*, *it's*, *there's*, *you'll*, *isn't*. Keep the long form
where *not* carries the stress ("it does **not** save"). Reference tables
and option lists can stay formal, as Angband's were. Don't contract every
sentence; a text that's all contractions is a pattern too.

### N18. Dashes

**Watch for:** em dashes (—), spaced en dashes ( – ), and ` -- ` between
words. Sphinx's smartquotes turns ` -- ` into an en dash and `---` into an
em dash, so they count too.

**Why:** the page dates em-dash overuse to 2022 to September 2026, and
notes a July 2026 study in which **Claude was the only current model that
still used em dashes more than professional writers**. Claude wrote this
manual.

**What to do:** don't swap every dash for a comma. The page's newest entry
says models that were stopped from using dashes now produce nothing but
commas and full stops, and that's become a sign of its own. Choose for
each one: a full stop, a colon before an explanation or list, brackets for
a real aside, *and*/*so*/*because* for a real relation, or a rewrite. Keep
dashes in number ranges (1–5) and as the "none" mark in table cells. Leave
`--option` flags alone.

### N19. Monotone rhythm (*weak*)

Every sentence 8 to 20 words, every clause joined by a comma (the
mid-2026 pattern). Mix short and long. A list is fine when there really
are several separate items.

---

## E. Formatting

### N21. Bold as decoration

**Watch for:** bold numbers (**73% of Angband's hit points**), bold sentences
as paragraph leads, bold on every term, bold advice (**Prefer this one.**).

**Why:** a habit from READMEs and slide decks: bolding every instance in a
"key takeaways" style.

**What to do:** keep bold for a term at the place it's defined, a label on
screen the player has to find, and a table's row labels. Remove the rest.
Bold-sentence leads become plain sentences, or headings if the section
really is split.

### N22. Inline-header lists

**Watch for:** `- **Label:** text` and `- **Label** — text` on every item.

**What to do:** if the labels carry no information beyond the text, write
prose or a plain list. If they're real names (option names, shop names), a
definition list or table is better than bold-plus-colon.

### N23. Title Case headings (*weak*)

Models capitalise every main word. **Don't change heading text in this
manual**: Sphinx builds anchors from headings, and pages link to them by
title (`` `Which Mac build do I want?`_ ``). If a heading really has to
change, grep for every reference and run the build.

### N24. Emoji and decorative arrows

Remove them. Arrows that are real UI notation (realm ▸ book ▸ spell) stay.

### N25. Markdown in RST

`[text](url)`, ```` ``` ```` fences, `## headings`, `---` between sections.
A strong sign when it shows up in RST, where it doesn't render. Convert it
to RST.

### N26. Curly quotes in the source (*weak*)

Smartquotes curls straight quotes at build time, so the source should have
straight ones. Claude rarely uses curly quotes anyway; ChatGPT and DeepSeek
do.

---

## F. Leftovers from the chat and the commit log

Remove these outright.

### N27. Chat residue

*I hope this helps*, *Certainly!*, *let me know*, *would you like*, *feel
free to*, *happy adventuring*.

### N28. Hedging about sources and use (new in mid-2026)

**Watch for:** *should be treated as X rather than Y*, *does not by itself
establish*, *based on available information*, *not widely documented*.

**Why:** the page lists this among the five commonest current signs, and its
own example is "disclosed as either ChatGPT or Claude."

**What to do:** state what's known. Cut the instruction about how to read it.

### N29. Didactic disclaimers

*It is important to note*, *worth noting*, *keep in mind*, *bear in mind*,
*may vary*. Usually the sentence works without the opener. Angband's own
documentation sometimes says "keep this in mind", so leave inherited text
alone.

### N30. Placeholders

*TODO*, *TBD*, *(if available)*, `20xx-xx-xx`. Fill them in from the code or
the data, or ask.

### N31. Process residue: edit-summary voice in a player's manual

**Watch for:** *has been checked against the game*, *has been corrected*,
*nothing else has been touched*, *kept as they were*, *preserved*,
*retained*, *this chapter describes what the game does today*, milestone
narration (*Milestone M10 is complete, all nine phases*), requirement IDs in
running prose (*the two pet sources PLR-28 named that the last of them
shipped without*), and *no longer*/*used to*/*now* about earlier builds of
this game.

**Why:** on Wikipedia this is the AI edit summary: "preserved", "ensured",
"in compliance with". It's how a model reports its work to the person who
asked for it. In this manual the same voice leaked out of commit messages
into pages a player reads.

**What to do:** describe the game, not the work on the page. Keep
requirement and DEC references where the page lists them (a
`Requirements: PLR-22 to PLR-33.` line is a cross-reference the project
keeps up to date), but take them out of sentences a player has to read
through. Keep "Angband does X, this game does Y" comparisons: those are
about the game. A note that says a chapter is inherited and what was
changed in it is fine. Say it in one or two plain sentences.

---

## Keep these: signs of human writing

The page lists constructions found **more** often in human writing than in
AI text. A rewrite that strips them makes the page *less* human, so leave
them in, and use them:

- plain *is*/*has* sentences: *there is a*, *it has a*
- plain verbs: *wrote*, *moved*, *used*, *tried*, *died*
- definite claims and superlatives: *the best*, *the only*, *the first*
- qualifiers and intensifiers: *very*, *perhaps*, *usually*, *tends to*
- the odd wordy phrase: *in order to*, *as a result of*, *the fact that*
- specific, odd details: "a search of Angband 4.2's source for the word
  *friendly* finds two hits, one a comment and one a shopkeeper's greeting"
  is exactly what models leave out. Keep every detail like that.

## Don't chase these: ineffective indicators

The page says these don't show AI use, and some point the other way:
perfect grammar; a mix of casual and formal registers (normal for a
technical writer); "bland" or "fancy" prose in general (only the specific
words in N13 count); transition words on their own; unsourced claims;
correct markup.

## Historical signs (rare now, still worth a look)

Knowledge-cutoff statements ("as of my last update"), *it's important to
note* (N29), "In conclusion" sections (N6), refusals ("as an AI language
model"), abrupt cut-offs mid-sentence, and elegant variation (N16).
