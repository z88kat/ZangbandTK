# Contributing to ZangbandTK

The last two sections — offering the change, and the coding style — are
Angband's, with the addresses corrected, because this is Angband's codebase and
there is no reason to differ. Everything before them is this project's, and it
is the part worth reading first: ZangbandTK has a handful of conventions that are
not obvious from the code, and a newcomer's first change tends to break one of
them.

Development happens at [z88kat/ZangbandTK](https://github.com/z88kat/ZangbandTK).
Upstream Angband is a different repository and its issue tracker is not ours.

## The archive is the authority

Zangband 2.7.5-pre1 is in [archive/zangband/](archive/zangband/), and Angband
2.8.1 — the common ancestor, so differences against it isolate what Zangband
actually changed — is in [archive/angband-281/](archive/angband-281/). Reading
them is not only allowed, it is the method: clean-room was dropped deliberately
(DEC-20), because several systems carry their value in the algorithm rather than
in the behaviour and a requirements document cannot convey them. Port where the
algorithm is the value; reimplement where 4.2's architecture differs.

**Where the archive says something, it decides — unless somebody has decided
otherwise, in writing.** Those decisions live in
[.claude/plans/decisions.md](.claude/plans/decisions.md), numbered, dated and
written when the call was made rather than reconstructed afterwards.

**A change that differs from Zangband with no decision entry behind it is the
single most common defect this project produces.** It does not look like a
defect. It looks like working code with a plausible number in it, and it is
found a year later by somebody comparing the two games and opening an
investigation into a faithful port. So when you diverge — because 4.2 has no
mechanism, because the archive's number transplants badly, because the spoiler
and the source disagree — add the entry. The standing test for one is whether a
later reader would find a ruling or an oversight.

Source comments carry the citation: the requirement ID, the decision number
where there is one, and a link to the archive line, like this, from
`src/mon-make.c`:

```c
/*
 * Five points faster in nightmare mode (BAL-15,
 * [dungeon.c:2833](../archive/zangband/src/dungeon.c#L2833)), capped at
 * 199 because that is the width of the energy table in both games.
 */
```

Two sources, not one: the official Zangband documentation states what was
*intended* and the source states what was *built*, and where they disagree the
documentation is usually the better guide to what was meant (DEC-16). Several
things the spoilers describe were never actually implemented, and importing them
as though they had been is its own kind of error.

## A test has to be able to fail

**Write the test, then break the code it covers, and confirm the test fails.**
Not "run it and watch it pass" — that says nothing. This project has shipped a
number of assertions that passed whether the code was right or wrong, and they
were caught by mutating the code rather than by reading the test. Mutation over
a sample is the only method here that has worked.

The shape recurs. A test asserts an *ingredient* of the behaviour instead of the
behaviour: the Vampire's glow checked `cur_light >= 1`, which the torch in the
starting kit satisfies on its own, so deleting the race's contribution broke
nothing. A helper named for a check only printed the reason and returned, so
eleven call sites were watching nothing. A savefile test read a value the loader
had never cleared, so it passed whatever was loaded. In each case the comment
above the test described the behaviour correctly and the assertion did not,
which is exactly what makes them invisible to a reader.

DEC-94 and DEC-99 are two of the write-ups, and they are worth reading once
before you write your first test here. The rule of thumb from DEC-99: a check
that passes and has never been seen to fail is indistinguishable from the
`printf` it replaced.

If a mutation does *not* fail the test, that is a finding about the test. Record
what you tried; several decision entries do.

## Run the gate, and wait for it

Both of the build commands in [README.md](README.md) are permissive. CI is not:
the macOS job builds with `-Werror` and every Linux job configures CMake with
it. A warning that scrolls past on your machine is fatal there, and that has let
defects reach master. It is not a weaker compiler here — it is the same
compiler, run more kindly.

```sh
scripts/check-build           # every build, the manual and the data checks
scripts/check-build --tests   # ...and run the unit tests after
```

Its exit code is the answer. It does not print a verdict for you to read past,
and nothing in it greps a build log for the word "error", because guessing at a
compiler's phrasing and guessing wrong reports success.

**The header of [scripts/check-build](scripts/check-build) is the authoritative
list of what it runs and why each pass is there.** Read it once. It is kept
current because the script is, which is not true of anything that restates it —
including this file.

Three things about running it, all of them learned expensively and all of them
in that header:

- **It takes a while with `--tests`, and it has to be waited on.** Backgrounding
  it and checking for a summary line later is how a result gets lost: if the
  phrase you grep for never appears — because the run failed early, or printed
  something else — the wait never ends and the outcome is never read. Poll the
  output file, and check whether the process is still alive rather than whether
  some phrase has shown up.
- **Do not run anything else against the build while it runs.** The suites are
  sequential by design and nothing in the harness collides with itself, but a
  suite run by hand alongside it does.
- **A green run says the tests passed once, not that they are deterministic.**
  `scripts/check-flakes -n 12` runs every suite twelve times and reports anything
  that was not the same every time.

## Two traps that will cost you an afternoon

Both of these produce a *passing* run that means nothing, which is what makes
them worth knowing before you meet them rather than after.

**`cmake --build build` does not build the test binaries.** The default target is
the game. A test binary left over from the previous build then runs and reports
the previous build's verdict, which looks exactly like a result. Build
`unittest-<suite>` or `run-unittest-<suite>`, and never hide the build output
behind `>/dev/null` while doing it: a build that failed and a build that was a
no-op are indistinguishable once silenced, and both leave you running yesterday's
binary.

**Data files staged into a build tree are compared by timestamp, at one-second
granularity.** Edit a file in `lib/gamedata`, build, then revert the edit within
the same second, and the staged copy keeps the edit for ever. The same applies
to restoring a source file with `cp` and rebuilding straight away — the restore
can look current and not recompile. The cost is not a failed run but a passing
one: the mutation stays in the binary, so the restore reports the mutation's
verdict and you draw a conclusion about code that is no longer there. Two
working guards were deleted on the strength of this before the pattern was
recognised. Pause, `touch` the file, and assert that it actually recompiled —
grep the build log for `Building C object .*<file>` — rather than assuming it.

## If behaviour changes, the manual changes

ZangbandTK ships its own manual in [docs/](docs/), because the game differs from
both ancestors in ways neither of their documents describes (DEC-17). A change a
player can notice is not finished until the relevant chapter says so. Sphinx
builds with `-W`, so a broken directive or a bad cross reference is a build
failure, and `scripts/check-build` builds the manual for that reason.

Adding a source file or a data file also means updating the build inputs that
are maintained by hand — the Visual Studio project, the DOS 8.3 renames, the
install list. `scripts/check-build-lists` names anything missing and needs only
a checkout.

## Offering the change

Fork [z88kat/ZangbandTK](https://github.com/z88kat/ZangbandTK) on GitHub, clone
your fork, and add the official repository as a second remote:

```sh
git clone https://github.com/yourlogin/ZangbandTK.git
cd ZangbandTK
git remote add official https://github.com/z88kat/ZangbandTK.git
```

Do not work in `master`. Branch per piece of work — branches are expendable and
a new one is a fresh start — and keep unrelated work in unrelated branches, or
the pull request gets messy and someone has to untangle it.

The loop:

1. `git fetch official && git checkout -b newbranch official/master`
2. ... do the work, and run the gate ...
3. `git fetch official` again, and `git rebase official/master` if it has moved
4. `git push origin newbranch`
5. Open the pull request, describing what it does, anything still outstanding,
   and the number of any issue it addresses

Rebase rather than merging `official/master` into your branch, so the history
does not fill with merge commits — but not after you have published the branch
or opened the request, since rebasing rewrites commit IDs and breaks anyone
tracking it. If you think of something after opening the request, commit to the
same branch and push again; GitHub adds it. After a merge, start the next piece
of work on a new branch from `official/master` rather than continuing on the old
one.

## Coding style

This is Angband's codebase and its conventions apply. The old
[Angband security guide](src/doc/security.txt) is still in the tree, although the
default build configuration no longer uses setgid.

* K&R brace style, tabs of four spaces
* Avoid lines over 80 characters (not strict under multiple indents, but that is
  usually a sign the function wants refactoring)
* A function taking no parameters is declared `function(void)`, not `function()`
* `const` where you are not modifying the variable
* Avoid global variables like the plague; there are already too many
* Enums rather than defines where possible, and never magic numbers
* No floating point
* Code compiles as C99 and must not rely on undefined behaviour
* Not the C string functions — the `my_` versions (`strcpy` → `my_strcpy`,
  `sprintf` → `strnfmt()`), which are safer

Indent style: opening braces on their own line at the start of a function and
otherwise on the line that requires them; closing braces on their own line
except before `while` or `else`; spaces around mathematical, comparison and
assignment operators but not around `++` and `--`; a space between `if`, `while`,
`for` and the opening bracket, and none between a function name and its bracket;
`return` without brackets, `sizeof` with them; two indents for a call or
condition continued onto another line.

```c
    if (fridge) {
        int i = 10;

        if (i > 100) {
            i += randint0(4);
            bar(1, 2);
        } else {
            foo(buf, sizeof(buf), FLAG_UNUSED, FLAG_TIMED,
                    FLAG_DEAD);
        }

        do {
            /* Only print even numbers */
            if (i % 2) continue;

            /* Be clever */
            printf("Aha!");
        } while (i--);

        return 5;
    }
```

Write code for humans first and execution second. Comment where code is unclear
— but `/* Delete the object */` above `object_delete(idx)` is noise, and this
project's comments are expected to carry the reasoning instead: why this number,
which archive line it came from, what was tried and rejected.

Write code as modules where possible, with functions and globals sharing a
prefix like `macro_`, and with `init` and `free` functions rather than
module-specific setup scattered elsewhere.

Function documentation:

```c
    /**
     * Provides an example of a documentation style.
     *
     * The purpose of the function do_something() is explained here, mentioning
     * the name and use of every parameter (e.g. `example`).  It returns true if
     * conditions X or Y are met, and false otherwise.
     */
    bool do_something(void *example)
```

The brief description is separated from the rest so Doxygen can pull it out
without an `@brief` tag. Refer to variables in backticks and to functions with
their brackets — `function_name()`. Present tense for what a function does; no
UK/US spelling preference, the first commenter's choice stands for that comment.
