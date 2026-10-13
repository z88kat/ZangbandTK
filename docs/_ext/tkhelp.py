"""The manual, as the Tk front end's Help window reads it.

The Help window draws the manual into a Tk text widget rather than rendering
HTML: TkHtml, which the original ZangbandTk used, will not build against Tk 9,
and the Phase 3 plan settled on a text-oriented builder instead (plan section
6, "help is Sphinx content, rendered inside the application").  This is that
builder.

    sphinx-build -b tkhelp docs lib/tcl/help

or scripts/build-tk-help, which also clears out pages that have gone.  The
output is committed, so a build of the game needs no Python; a workflow checks
that it is current.

Only the player's manual goes in: the parts named in ``tkhelp_roots`` and the
chapters under them.  The website's own pages, the diary and the developer
notes do not, and a link to one of them becomes a link to the website.

Each page is one file, one instruction a line, fields separated by tabs:

    H   title                       the page's title
    T   tags  text                  insert text with those tags
    L   tags  id  target  text      insert a link: target is "doc", "doc#id",
                                    "#id" on the same page, or a URL
    M   id                          an anchor where the next text goes

Text has its newlines, tabs and backslashes written as \\n, \\t and \\\\.  Tags
are space-separated: h1 to h4, em, strong, code, note and warn for the boxes,
and block tags ending in the indent level -- p0, li1, dt0, pre1, tr0, th0,
rule0 -- which the window turns into margins.  All of the layout is decided
here, so the window only has to insert what it is given.

contents.txt is the tree for the Contents tab, in the shape of the original's
tk/doc/contents:

    book  title  target             opens a level
    page  title  target
    stop                            closes it
"""

import os
import re
import shutil
import textwrap

from docutils import nodes
from sphinx import addnodes
from sphinx.builders import Builder
from sphinx.util import logging

logger = logging.getLogger(__name__)

# The widest a table is laid out, in characters of the fixed font.  Wider than
# this and a column's text is wrapped within it.
TABLE_WIDTH = 96

BOXES = {
    "note": ("Note", "note"),
    "tip": ("Tip", "note"),
    "hint": ("Hint", "note"),
    "important": ("Important", "note"),
    "seealso": ("See also", "note"),
    "warning": ("Warning", "warn"),
    "caution": ("Caution", "warn"),
    "attention": ("Attention", "warn"),
    "danger": ("Danger", "warn"),
    "error": ("Error", "warn"),
}


def findall(node, kind):
    """Descendants of a kind: docutils 0.18 renamed traverse to findall."""
    if hasattr(node, "findall"):
        return list(node.findall(kind))
    return node.traverse(kind)


def escape(text):
    return (text.replace("\\", "\\\\").replace("\t", "\\t")
            .replace("\n", "\\n"))


def scope_of(env, roots):
    """The pages that go in, roots first, each followed by its chapters."""
    seen = []

    def add(doc):
        if doc in seen or doc not in env.found_docs:
            return
        seen.append(doc)
        for child in env.toctree_includes.get(doc, []):
            add(child)

    for root in roots:
        add(root)
    return seen


class PageWriter(nodes.SparseNodeVisitor):
    """Walks one resolved page and writes its instructions."""

    def __init__(self, builder, docname, document):
        super().__init__(document)
        self.builder = builder
        self.docname = docname
        self.lines = []
        self.sections = []
        self.indent = 0
        self.depth = 0
        self.inline = []
        self.box = None
        self.block = None
        self.bullet = None
        self.lists = []
        self.links = []
        self.link_count = 0
        self.fresh = True

    # --- output

    def tags(self, extra=()):
        tags = [self.block] if self.block else []
        if self.box:
            tags.append(self.box)
        tags.extend(self.inline)
        tags.extend(extra)
        return " ".join(tags)

    def text(self, text, extra=()):
        if text:
            self.lines.append("T\t%s\t%s" % (self.tags(extra), escape(text)))

    def mark(self, name):
        self.lines.append("M\t%s" % name)

    def start(self, kind):
        """Open a block -- a line of its own in the text widget.

        A block tag is its kind and the indent level, p2 or li1, except a
        heading's, which is its level alone.
        """
        self.end()
        if self.bullet is not None and kind == "p":
            kind = "li"
        self.block = kind if kind.startswith("h") else "%s%d" % (kind, self.indent)
        if kind == "li":
            bullet, self.bullet = self.bullet, None
            self.text(bullet + "\t")
        self.fresh = True

    def end(self):
        if self.block:
            self.text("\n")
            self.block = None

    # --- every element

    def dispatch_visit(self, node):
        # Anything can be the target of a link: write its ids as anchors.
        # A section's go in before its heading, so the heading is in view
        # when the link is followed.
        if isinstance(node, nodes.Element):
            for name in node.get("ids", []):
                self.mark(name)
        return super().dispatch_visit(node)

    def unknown_visit(self, node):
        pass

    def unknown_departure(self, node):
        pass

    # --- structure

    def visit_document(self, node):
        title = self.builder.env.titles.get(self.docname)
        self.lines.append("H\t%s" % escape(title.astext() if title else ""))

    def depart_document(self, node):
        self.end()

    def visit_section(self, node):
        self.depth += 1

    def depart_section(self, node):
        self.depth -= 1

    def visit_title(self, node):
        parent = node.parent
        if isinstance(parent, nodes.section):
            self.start("h%d" % min(self.depth, 4))
            if self.depth == 2 and parent.get("ids"):
                # A section with an index entry carries index-6 as well as
                # its own name; the contents should point at the name.
                ids = [i for i in parent["ids"]
                       if not re.match(r"index-\d+$", i)] or parent["ids"]
                self.sections.append((ids[0], node.astext()))
        elif isinstance(parent, (nodes.table, nodes.topic, nodes.sidebar,
                                 nodes.Admonition)):
            self.start("p")
            self.inline.append("strong")
        else:
            self.start("p")

    def depart_title(self, node):
        if "strong" in self.inline:
            self.inline.remove("strong")
        self.end()

    def visit_paragraph(self, node):
        self.start("p")

    def depart_paragraph(self, node):
        self.end()

    visit_compact_paragraph = visit_paragraph
    depart_compact_paragraph = depart_paragraph

    def visit_Text(self, node):
        if self.block is None:
            self.start("p")
        text = re.sub(r"\s+", " ", node.astext())
        if self.fresh:
            text = text.lstrip()
        if not text:
            return
        self.fresh = False
        link = next((l for l in reversed(self.links) if l), None)
        if link:
            link_id, target = link
            self.lines.append("L\t%s\t%d\t%s\t%s" % (
                self.tags(["link"]), link_id, escape(target), escape(text)))
        else:
            self.text(text)

    # --- lists

    def visit_bullet_list(self, node):
        self.end()
        self.indent += 1
        self.lists.append(None)

    def depart_bullet_list(self, node):
        self.end()
        self.indent -= 1
        self.lists.pop()

    def visit_enumerated_list(self, node):
        self.end()
        self.indent += 1
        self.lists.append(node.get("start", 1))

    depart_enumerated_list = depart_bullet_list

    def visit_list_item(self, node):
        self.end()
        n = self.lists[-1] if self.lists else None
        if n is None:
            self.bullet = "•"
        else:
            self.bullet = "%d." % n
            self.lists[-1] = n + 1

    def depart_list_item(self, node):
        self.end()
        self.bullet = None

    def visit_definition_list_item(self, node):
        self.end()

    def visit_term(self, node):
        self.start("dt")
        self.inline.append("strong")

    def depart_term(self, node):
        self.inline.remove("strong")
        self.end()

    def visit_classifier(self, node):
        raise nodes.SkipNode

    def visit_definition(self, node):
        self.end()
        self.indent += 1

    def depart_definition(self, node):
        self.end()
        self.indent -= 1

    def visit_block_quote(self, node):
        self.end()
        self.indent += 1

    depart_block_quote = depart_definition

    def visit_line_block(self, node):
        self.end()
        if isinstance(node.parent, nodes.line_block):
            self.indent += 1

    def depart_line_block(self, node):
        self.end()
        if isinstance(node.parent, nodes.line_block):
            self.indent -= 1

    def visit_line(self, node):
        self.start("ln")

    def depart_line(self, node):
        self.end()

    def visit_transition(self, node):
        self.start("rule")
        self.text("─" * 40)
        self.end()
        raise nodes.SkipNode

    # --- boxes

    def visit_admonition(self, node):
        self.end()
        self.box = "note"

    def depart_admonition(self, node):
        self.end()
        self.box = None

    def generic_box(self, node):
        name = node.__class__.__name__
        title, box = BOXES.get(name, (name.title(), "note"))
        self.end()
        self.box = box
        self.start("p")
        self.inline.append("strong")
        self.text(title)
        self.inline.remove("strong")
        self.end()

    def visit_topic(self, node):
        self.end()
        self.box = "note"

    depart_topic = depart_admonition
    visit_sidebar = visit_topic
    depart_sidebar = depart_admonition

    # --- inline

    def push(self, tag):
        self.inline.append(tag)

    def pop(self, tag):
        if tag in self.inline:
            self.inline.reverse()
            self.inline.remove(tag)
            self.inline.reverse()

    def visit_emphasis(self, node):
        self.push("em")

    def depart_emphasis(self, node):
        self.pop("em")

    visit_title_reference = visit_emphasis
    depart_title_reference = depart_emphasis

    def visit_strong(self, node):
        self.push("strong")

    def depart_strong(self, node):
        self.pop("strong")

    def visit_literal(self, node):
        self.push("code")

    def depart_literal(self, node):
        self.pop("code")

    def visit_literal_strong(self, node):
        self.push("code")
        self.push("strong")

    def depart_literal_strong(self, node):
        self.pop("strong")
        self.pop("code")

    def visit_literal_emphasis(self, node):
        self.push("code")
        self.push("em")

    def depart_literal_emphasis(self, node):
        self.pop("em")
        self.pop("code")

    def visit_reference(self, node):
        target = self.target(node)
        if target is None:
            self.links.append(None)
            return
        self.link_count += 1
        self.links.append((self.link_count, target))

    def depart_reference(self, node):
        self.links.pop()

    def target(self, node):
        if "refid" in node:
            return "#" + node["refid"]
        uri = node.get("refuri")
        if not uri:
            return None
        if re.match(r"^[a-z][a-z0-9+.-]*:", uri):
            return uri
        doc, _, anchor = uri.partition("#")
        if not doc:
            return "#" + anchor
        if doc in self.builder.scope:
            return uri
        # A page the Help window does not carry: the website has it.
        site = self.builder.config.html_baseurl or ""
        if not site.endswith("/"):
            site += "/"
        return site + doc + ".html" + ("#" + anchor if anchor else "")

    # --- whole elements, laid out here

    def visit_literal_block(self, node):
        self.end()
        lines = node.astext().split("\n")
        for i, line in enumerate(lines):
            self.block = "pre%d" % self.indent
            extra = ["prelast"] if i == len(lines) - 1 else []
            self.text(line + "\n", extra)
            self.block = None
        raise nodes.SkipNode

    visit_doctest_block = visit_literal_block

    def visit_table(self, node):
        self.end()
        title = node.next_node(nodes.title)
        if title is not None and title.parent is node:
            self.start("p")
            self.inline.append("strong")
            self.text(re.sub(r"\s+", " ", title.astext()))
            self.inline.remove("strong")
            self.end()
        head, body = [], []
        for group in findall(node, nodes.thead):
            head.extend(self.rows(group))
        for group in findall(node, nodes.tbody):
            body.extend(self.rows(group))
        self.layout(head, body)
        raise nodes.SkipNode

    def rows(self, group):
        rows = []
        for row in findall(group, nodes.row):
            cells = []
            for entry in findall(row, nodes.entry):
                cells.append(re.sub(r"\s+", " ", entry.astext()).strip())
                cells.extend([""] * entry.get("morecols", 0))
            rows.append(cells)
        return rows

    def layout(self, head, body):
        if not head and not body:
            return
        ncols = max(len(r) for r in head + body)
        head = [r + [""] * (ncols - len(r)) for r in head]
        body = [r + [""] * (ncols - len(r)) for r in body]
        rows = head + body
        gap = 2
        widest = [max(len(r[c]) for r in rows) for c in range(ncols)]
        longest = [max([len(w) for r in rows for w in r[c].split()] or [0])
                   for c in range(ncols)]
        widths = [max(w, 1) for w in widest]
        budget = TABLE_WIDTH - gap * (ncols - 1)
        # Narrow whichever column has most to give, a character at a time,
        # until the table fits -- but none below its longest word, or twenty
        # if that is longer, when the word is broken.  A table that still
        # does not fit is left wide, and the window scrolls sideways.
        floor = [max(1, min(widths[c], longest[c], 20)) for c in range(ncols)]
        while sum(widths) > budget:
            c = max(range(ncols), key=lambda i: widths[i] - floor[i])
            if widths[c] <= floor[c]:
                break
            widths[c] -= 1

        def emit(row, header, stripe):
            wrapped = [textwrap.wrap(row[c], widths[c],
                                     break_long_words=True,
                                     break_on_hyphens=False) or [""]
                       for c in range(ncols)]
            height = max(len(w) for w in wrapped)
            for k in range(height):
                parts = []
                for c in range(ncols):
                    piece = wrapped[c][k] if k < len(wrapped[c]) else ""
                    parts.append(piece.ljust(widths[c]))
                line = (" " * gap).join(parts).rstrip()
                self.block = "%s%d" % ("th" if header else "tr", self.indent)
                self.text(line + "\n", [stripe] if stripe else [])
                self.block = None

        for row in head:
            emit(row, True, None)
        if head:
            self.block = "rule%d" % self.indent
            self.text("─" * (sum(widths) + gap * (ncols - 1)) + "\n")
            self.block = None
        for i, row in enumerate(body):
            emit(row, False, "odd" if i % 2 else None)
        self.block = "tbllast%d" % self.indent
        self.text("\n")
        self.block = None

    # --- left out

    def skip(self, node):
        raise nodes.SkipNode

    visit_comment = skip
    visit_substitution_definition = skip
    visit_index = skip
    visit_raw = skip
    visit_system_message = skip
    visit_image = skip
    visit_figure = skip
    visit_meta = skip
    visit_toctree = skip


for _name in BOXES:
    setattr(PageWriter, "visit_" + _name, PageWriter.generic_box)
    setattr(PageWriter, "depart_" + _name, PageWriter.depart_admonition)


class TkHelpBuilder(Builder):
    """Writes the Help window's pages and its contents."""

    name = "tkhelp"
    format = "tkhelp"
    epilog = "The Help window's pages are in %(outdir)s."
    allow_parallel = False

    def init(self):
        self.scope = []
        self.sections = {}

    def get_outdated_docs(self):
        return self.env.found_docs

    # Links between pages are by name -- "birth", "birth#races" -- since the
    # window looks pages up by name rather than following paths.
    def get_target_uri(self, docname, typ=None):
        return docname

    def get_relative_uri(self, from_, to, typ=None):
        return self.get_target_uri(to, typ)

    def prepare_writing(self, docnames):
        self.scope = scope_of(self.env, self.config.tkhelp_roots)

    def page_file(self, docname):
        return os.path.join(self.outdir, docname.replace("/", "-") + ".txt")

    def write_doc(self, docname, doctree):
        if docname not in self.scope:
            return
        writer = PageWriter(self, docname, doctree)
        doctree.walkabout(writer)
        self.sections[docname] = writer.sections
        with open(self.page_file(docname), "w", encoding="utf-8",
                  newline="\n") as f:
            f.write("\n".join(writer.lines) + "\n")

    def finish(self):
        lines = []

        def title(doc):
            t = self.env.titles.get(doc)
            return t.astext() if t else doc

        def walk(doc):
            children = [c for c in self.env.toctree_includes.get(doc, [])
                        if c in self.scope]
            sections = self.sections.get(doc, [])
            if not children and not sections:
                lines.append("page\t%s\t%s" % (title(doc), doc))
                return
            lines.append("book\t%s\t%s" % (title(doc), doc))
            for anchor, text in sections:
                lines.append("page\t%s\t%s#%s" % (text, doc, anchor))
            for child in children:
                walk(child)
            lines.append("stop")

        for root in self.config.tkhelp_roots:
            if root in self.scope:
                walk(root)
        with open(os.path.join(self.outdir, "contents.txt"), "w",
                  encoding="utf-8", newline="\n") as f:
            f.write("\n".join(lines) + "\n")

        # Pages that are no longer in the manual go, so the window cannot
        # find one by an old name.
        keep = {os.path.basename(self.page_file(d)) for d in self.scope}
        keep.add("contents.txt")
        for name in os.listdir(self.outdir):
            if name.endswith(".txt") and name not in keep:
                os.remove(os.path.join(self.outdir, name))

        # sphinx_design puts its stylesheet in every builder's output as the
        # builder starts, whether it writes HTML or not.
        shutil.rmtree(os.path.join(self.outdir, "_sphinx_design_static"),
                      ignore_errors=True)


def setup(app):
    app.add_config_value("tkhelp_roots", ["start", "manual", "differences",
                                          "meta"], "env")
    app.add_builder(TkHelpBuilder)
    return {"version": "1", "parallel_read_safe": True,
            "parallel_write_safe": False}
