"""The manual as one book, for scripts/book/build.py to lay out and print.

    sphinx-build -b book docs <outdir>

writes <outdir>/book.html, every chapter of the book in order, and
<outdir>/book.json, which says what is in it.  Neither is a page anyone
reads: build.py puts them in the book's own page, with its title page,
contents, running heads and pictures, and has Chrome print that.

Which chapters go in, in what order and under which heading of the contents,
is ``book_sections`` in conf.py.  Pages not in the book -- the website's own,
the diary, the developer notes -- are linked to on the website, so a link to
one from the book still goes somewhere.

The chapters are written by Sphinx's own HTML writer, so everything the site
can show the book can too.  Two things are done to what it writes:

- Every id is given its chapter's name, "birth--races", because two chapters
  can each have a section called the same thing, and in one file that is two
  ids the same.  Links are rewritten to match.
- A link to another chapter becomes a link within the book.
"""

import json
import os
import re

from sphinx.builders.html import StandaloneHTMLBuilder
from sphinx.util import logging

logger = logging.getLogger(__name__)

# What a link to a chapter in the book looks like until finish() resolves it.
MARK = "BOOK-CHAPTER:"


class BookBuilder(StandaloneHTMLBuilder):
    """Collects each chapter's body, and writes them as one."""

    name = "book"
    format = "html"
    epilog = "The book's chapters are in %(outdir)s/book.html."
    allow_parallel = False
    search = False
    copysource = False
    add_permalinks = False

    def init(self):
        super().init()
        self.bodies = {}
        self.titles = {}

    def chapters(self):
        return [doc for _, docs in self.config.book_sections for doc in docs]

    def get_outdated_docs(self):
        return self.env.found_docs

    def get_target_uri(self, docname, typ=None):
        if docname in self.chapters():
            return MARK + docname
        site = self.config.html_baseurl or ""
        if not site.endswith("/"):
            site += "/"
        return site + docname + ".html"

    def get_relative_uri(self, from_, to, typ=None):
        return self.get_target_uri(to, typ)

    # StandaloneHTMLBuilder.write_doc renders the chapter and hands it here to
    # be put in a template; this keeps the body instead.
    def handle_page(self, pagename, addctx, templatename="page.html",
                    outfilename=None, event_arg=None):
        if pagename not in self.chapters():
            return
        self.bodies[pagename] = addctx.get("body", "")
        self.titles[pagename] = addctx.get("title", pagename)

    def write_doc(self, docname, doctree):
        if docname not in self.chapters():
            return
        super().write_doc(docname, doctree)

    def chapter_html(self, doc):
        body = self.bodies[doc]

        def own_id(m):
            return '%s="%s--%s"' % (m.group(1), doc, m.group(2))

        body = re.sub(r'\b(id)="([^"]+)"', own_id, body)

        def link(m):
            target = m.group(1)
            if target.startswith(MARK):
                rest = target[len(MARK):]
                other, _, anchor = rest.partition("#")
                return 'href="#%s--%s"' % (other, anchor)
            if target.startswith("#"):
                return 'href="#%s--%s"' % (doc, target[1:])
            return m.group(0)

        body = re.sub(r'href="([^"]*)"', link, body)
        return ('<section class="chapter" id="%s--" data-doc="%s">\n%s\n'
                '</section>\n' % (doc, doc, body))

    def finish(self):
        missing = [d for d in self.chapters() if d not in self.bodies]
        for doc in missing:
            logger.warning("book: no chapter called %s", doc)
        with open(os.path.join(self.outdir, "book.html"), "w",
                  encoding="utf-8", newline="\n") as f:
            for doc in self.chapters():
                if doc in self.bodies:
                    f.write(self.chapter_html(doc))
        info = {
            "sections": [[title, [d for d in docs if d in self.bodies]]
                         for title, docs in self.config.book_sections],
            "titles": {d: self.titles[d] for d in self.bodies},
        }
        with open(os.path.join(self.outdir, "book.json"), "w",
                  encoding="utf-8", newline="\n") as f:
            json.dump(info, f, indent=1)


def setup(app):
    app.add_config_value("book_sections", [], "env")
    app.add_builder(BookBuilder)
    return {"version": "1", "parallel_read_safe": True,
            "parallel_write_safe": False}
