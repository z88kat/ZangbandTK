# The Help window.
#
# T9: the manual, read inside the game, as the original ZangbandTk's Help
# window read its HTML -- a Contents tree beside the page, Back and Next, and
# a Section menu of the chapters.  The original drew the pages with TkHtml,
# which will not build against Tk 9, so the plan settled on drawing the Sphinx
# manual into a text widget instead (section 6, "help is Sphinx content,
# rendered inside the application").
#
# The pages are made from docs/ by scripts/build-tk-help and kept in
# lib/tcl/help, one file a page; docs/_ext/tkhelp.py, the builder, describes
# the format.  Everything about layout -- indents, bullets, table columns --
# is decided there, so this file only inserts what it is given and gives the
# tags their fonts and margins.  contents.txt is the Contents tree.
#
# Help ▸ Help opens it, and so does the game's ? key: do_cmd_help asks the
# front end, through help_view_hook, rather than showing its text files.

set helpwin(dir) [file join [file dirname [info script]] help]
set helpwin(site) https://zangbandtk.com/
set helpwin(history) {}
set helpwin(at) -1
set helpwin(doc) ""
set helpwin(query) ""
set helpwin(hits) {}
set helpwin(hit) -1
set helpwin(shown) 1
set helpwin(status) ""
set helpwin(section) ""

# Sized as the windows' text is, so the Display page's Window text sets them
# too (display.tcl).
foreach {name family size styles} {
    helpbody   Helvetica 13 {}
    helpem     Helvetica 13 italic
    helpstrong Helvetica 13 bold
    helpboth   Helvetica 13 {bold italic}
    helpcode   Courier   13 {}
    helpcodeb  Courier   13 bold
    helph1     Helvetica 22 bold
    helph2     Helvetica 17 bold
    helph3     Helvetica 14 bold
    helph4     Helvetica 13 bold
} {
    ui_font_create $name $family $size {*}$styles
}

# --- the pages ---------------------------------------------------------------

# A page's instructions, as {kind field ...} lists; read once and kept.  The
# home page, @contents, is made here from the Contents tree, as the original
# made its Table of Contents.
proc help_page {doc} {
    global helpwin

    if {[info exists helpwin(page,$doc)]} { return $helpwin(page,$doc) }
    if {$doc eq "@contents"} {
        set helpwin(page,$doc) [help_contents_page]
        return $helpwin(page,$doc)
    }
    set path [file join $helpwin(dir) [string map {/ -} $doc].txt]
    if {[catch {open $path r} f]} { return {} }
    fconfigure $f -encoding utf-8
    set data [read $f]
    close $f

    set unescape [list "\\n" "\n" "\\t" "\t" "\\\\" "\\"]
    set ops {}
    foreach line [split $data \n] {
        set fields [split $line \t]
        switch -- [lindex $fields 0] {
            H { lappend ops [list H [string map $unescape [lindex $fields 1]]] }
            T {
                lappend ops [list T [lindex $fields 1] \
                    [string map $unescape [lindex $fields 2]]]
            }
            L {
                lassign $fields kind tags id target text
                lappend ops [list L $tags $id [string map $unescape $target] \
                    [string map $unescape $text]]
            }
            M { lappend ops [list M [lindex $fields 1]] }
        }
    }
    set helpwin(page,$doc) $ops
    return $ops
}

proc help_title {doc} {
    if {$doc eq "@contents"} { return Contents }
    set first [lindex [help_page $doc] 0]
    if {[lindex $first 0] eq "H"} { return [lindex $first 1] }
    return $doc
}

# A page's words without their markup, for searching it.
proc help_plain {doc} {
    global helpwin

    if {[info exists helpwin(plain,$doc)]} { return $helpwin(plain,$doc) }
    set text ""
    foreach op [help_page $doc] {
        switch -- [lindex $op 0] {
            T { append text [lindex $op 2] }
            L { append text [lindex $op 4] }
        }
    }
    set helpwin(plain,$doc) $text
    return $text
}

# The Contents tree: a list of {kind title target children}, kind book or page.
proc help_contents {} {
    global helpwin

    if {[info exists helpwin(contents)]} { return $helpwin(contents) }
    set stack [list {}]
    if {![catch {open [file join $helpwin(dir) contents.txt] r} f]} {
        fconfigure $f -encoding utf-8
        foreach line [split [read $f] \n] {
            set fields [split $line \t]
            switch -- [lindex $fields 0] {
                book {
                    lappend stack [list book [lindex $fields 1] \
                        [lindex $fields 2]]
                    lappend stack {}
                }
                page {
                    set top [lindex $stack end]
                    lappend top [list page [lindex $fields 1] \
                        [lindex $fields 2] {}]
                    lset stack end $top
                }
                stop {
                    if {[llength $stack] < 3} continue
                    set children [lindex $stack end]
                    set book [lindex $stack end-1]
                    set stack [lrange $stack 0 end-2]
                    set top [lindex $stack end]
                    lappend top [list {*}$book $children]
                    lset stack end $top
                }
            }
        }
        close $f
    }
    set helpwin(contents) [lindex $stack 0]
    return $helpwin(contents)
}

# Every page the Contents names, in its order.
proc help_docs {} {
    set docs {}
    set queue [help_contents]
    while {[llength $queue]} {
        set queue [lassign $queue node]
        lassign $node kind title target children
        set doc [lindex [split $target #] 0]
        if {$doc ni $docs} { lappend docs $doc }
        set queue [list {*}$children {*}$queue]
    }
    return $docs
}

proc help_contents_page {} {
    set ops [list [list H Contents] [list T h1 "Contents\n"]]
    set contents [help_contents]
    if {![llength $contents]} {
        lappend ops [list T p0 "The manual is not in this copy of the game.\
            Help ▸ Open in Browser reads it on the website.\n"]
        return $ops
    }
    lappend ops [list T p0 "The manual for ZangbandTK, in four parts.\
        Choose a chapter here or in the Contents beside this page, or look\
        for a word in all of them from Search.\n"]
    set id 0
    foreach part $contents {
        lassign $part kind title target children
        lappend ops [list L "h2 link" [incr id] $target $title] \
            [list T h2 "\n"]
        foreach chapter $children {
            lassign $chapter kind title target
            if {[string match *#* $target]} continue
            lappend ops [list T li1 "•\t"] \
                [list L "li1 link" [incr id] $target $title] \
                [list T li1 "\n"]
        }
    }
    return $ops
}

# --- drawing a page ----------------------------------------------------------

# The block tags carry an indent level, li2 or pre1, and are made the first
# time one is asked for: margins of a left edge, then a step a level.
proc help_tag {t tag} {
    if {[lsearch -exact [$t tag names] $tag] >= 0} return
    set edge 14
    set step 26
    if {![regexp {^([a-z]+)(\d+)$} $tag -> kind n]} return
    set left [expr {$edge + $n * $step}]
    switch -- $kind {
        p   { $t tag configure $tag -lmargin1 $left -lmargin2 $left -spacing3 8 }
        ln  { $t tag configure $tag -lmargin1 $left -lmargin2 $left }
        li  {
            $t tag configure $tag -lmargin1 [expr {$left - $step + 8}] \
                -lmargin2 $left -tabs [list $left] -spacing3 4
        }
        dt  {
            $t tag configure $tag -lmargin1 $left -lmargin2 $left \
                -spacing1 6 -spacing3 2
        }
        pre {
            $t tag configure $tag -lmargin1 [expr {$left + 10}] \
                -lmargin2 [expr {$left + 10}] -font helpcode -wrap none \
                -background #f3f3f3
        }
        tr  {
            $t tag configure $tag -lmargin1 $left -lmargin2 $left \
                -font helpcode -wrap none
        }
        th  {
            $t tag configure $tag -lmargin1 $left -lmargin2 $left \
                -font helpcodeb -wrap none -spacing1 4
        }
        rule {
            $t tag configure $tag -lmargin1 $left -lmargin2 $left \
                -font helpcode -wrap none -foreground #a0a0a0
        }
        tbllast { $t tag configure $tag -spacing3 4 }
    }
    # Made after the inline tags, a block tag would win over them; lowered,
    # an inline font or colour still shows inside it.
    $t tag lower $tag
}

proc help_text_tags {t} {
    foreach {tag opts} {
        h1 {-font helph1 -spacing1 6 -spacing3 12 -lmargin1 14 -lmargin2 14}
        h2 {-font helph2 -spacing1 20 -spacing3 6 -lmargin1 14 -lmargin2 14}
        h3 {-font helph3 -spacing1 14 -spacing3 4 -lmargin1 14 -lmargin2 14}
        h4 {-font helph4 -spacing1 10 -spacing3 2 -lmargin1 14 -lmargin2 14}
        note {-background #eaf1fb -lmargincolor #eaf1fb}
        warn {-background #fbeee6 -lmargincolor #fbeee6}
        odd {-background #f1f1f1}
        prelast {-spacing3 10}
        em {-font helpem}
        strong {-font helpstrong}
        code {-font helpcode -foreground #702020}
        both {-font helpboth}
        codeb {-font helpcodeb -foreground #702020}
        link {-foreground #1a4fa0 -underline 1}
        found {-background #ffe680}
        here {-background #ffb84d}
    } {
        $t tag configure $tag {*}$opts
    }
    $t tag bind link <Enter> [list help_link_enter $t]
    $t tag bind link <Leave> [list help_link_leave $t]
    $t tag bind link <ButtonRelease-1> [list help_link_click $t %x %y]
}

# The tags of one instruction, with the block tags made and the two
# combinations a text widget cannot otherwise show -- bold italic, and bold
# code -- given tags of their own.
proc help_tags {t tags} {
    foreach tag $tags { help_tag $t $tag }
    if {"em" in $tags && "strong" in $tags} { lappend tags both }
    if {"code" in $tags && "strong" in $tags} { lappend tags codeb }
    return $tags
}

proc help_render {doc} {
    global helpwin

    set t .help.f.pw.page.t
    $t configure -state normal
    $t delete 1.0 end
    foreach mark [$t mark names] {
        if {[string match a:* $mark]} { $t mark unset $mark }
    }
    foreach tag [$t tag names] {
        if {[string match l:* $tag]} { $t tag delete $tag }
    }
    array unset helpwin link,*

    foreach op [help_page $doc] {
        switch -- [lindex $op 0] {
            T {
                lassign $op kind tags text
                $t insert end $text [help_tags $t $tags]
            }
            L {
                lassign $op kind tags id target text
                set helpwin(link,l:$id) $target
                $t insert end $text [list {*}[help_tags $t $tags] l:$id]
            }
            M {
                # Left gravity, so the text inserted next goes after it.
                $t mark set a:[lindex $op 1] end-1c
                $t mark gravity a:[lindex $op 1] left
            }
        }
    }
    $t configure -state disabled
    set helpwin(doc) $doc
}

# --- following links -----------------------------------------------------------

proc help_link_at {t x y} {
    global helpwin
    foreach tag [$t tag names @$x,$y] {
        if {[info exists helpwin(link,$tag)]} { return $helpwin(link,$tag) }
    }
    return ""
}

proc help_link_enter {t} {
    global helpwin
    $t configure -cursor hand2
    set x [expr {[winfo pointerx $t] - [winfo rootx $t]}]
    set y [expr {[winfo pointery $t] - [winfo rooty $t]}]
    set target [help_link_at $t $x $y]
    if {[help_external $target]} {
        set helpwin(status) $target
    } elseif {$target ne ""} {
        lassign [help_split $target] doc anchor
        set helpwin(status) [help_title $doc]
    }
}

proc help_link_leave {t} {
    global helpwin
    $t configure -cursor arrow
    set helpwin(status) [help_title $helpwin(doc)]
}

proc help_link_click {t x y} {
    # A drag that ends on a link is a selection, not a click.
    if {[llength [$t tag ranges sel]]} return
    set target [help_link_at $t $x $y]
    if {$target ne ""} { help_show $target }
}

proc help_external {target} {
    return [regexp {^[a-z][a-z0-9+.-]*:} $target]
}

# {doc anchor} from a target: "birth#races", "birth", "#races" on the page
# being read, or "" for the home page.
proc help_split {target} {
    global helpwin
    set at [string first # $target]
    if {$at < 0} {
        set doc $target
        set anchor ""
    } else {
        set doc [string range $target 0 [expr {$at - 1}]]
        set anchor [string range $target [expr {$at + 1}] end]
    }
    if {$doc eq ""} {
        set doc [expr {$helpwin(doc) ne "" ? $helpwin(doc) : "@contents"}]
    }
    return [list $doc $anchor]
}

# The first of some targets that the manual has: the page, and the place on it
# when one is named.  For the windows' F1, which each know what they would
# like to show and have to settle for what is there -- a realm of magic the
# manual has no section for gets its chapter instead.
proc help_first {args} {
    foreach target $args {
        if {$target eq ""} continue
        set at [string first # $target]
        set doc [expr {$at < 0 ? $target : [string range $target 0 $at-1]}]
        set anchor [expr {$at < 0 ? "" : [string range $target $at+1 end]}]
        if {$doc eq "" || ![llength [help_page $doc]]} continue
        if {$anchor eq ""} { return $doc }
        foreach op [help_page $doc] {
            if {[lindex $op 0] eq "M" && [lindex $op 1] eq $anchor} {
                return $target
            }
        }
    }
    return @contents
}

proc help_open_url {url} {
    global helpwin
    switch -- [tk windowingsystem] {
        aqua    { set cmd [list open $url] }
        win32   { set cmd [list {*}[auto_execok start] {} $url] }
        default { set cmd [list xdg-open $url] }
    }
    if {[catch {exec {*}$cmd &} err]} {
        set helpwin(status) "Could not open $url: $err"
    } else {
        set helpwin(status) "Opened $url in the browser."
    }
}

# The page being read, on the website: for a table the window draws too
# narrow, or to read it somewhere else.
proc help_browser {} {
    global helpwin
    set page [expr {$helpwin(doc) in {"" @contents} ? "documentation" : $helpwin(doc)}]
    help_open_url "$helpwin(site)$page.html"
}

# --- going somewhere -------------------------------------------------------------

# Show a page, and the anchor on it if there is one.  record is 0 for Back and
# Next, which move through the history rather than adding to it.
proc help_show {target {record 1}} {
    global helpwin

    if {[help_external $target]} {
        help_open_url $target
        return
    }
    if {![winfo exists .help]} { help_window }
    lassign [help_split $target] doc anchor
    if {![llength [help_page $doc]]} {
        set helpwin(status) "There is no page called $doc."
        bell
        return
    }
    if {$doc ne $helpwin(doc)} { help_render $doc }

    set t .help.f.pw.page.t
    if {$anchor ne "" && "a:$anchor" in [$t mark names]} {
        $t yview a:$anchor
    } else {
        $t yview moveto 0
    }

    if {$record} {
        set here [list $doc $anchor]
        if {[lindex $helpwin(history) $helpwin(at)] ne $here} {
            set helpwin(history) [lrange $helpwin(history) 0 $helpwin(at)]
            lappend helpwin(history) $here
            set helpwin(at) [expr {[llength $helpwin(history)] - 1}]
        }
    }
    set helpwin(section) $doc
    set helpwin(status) [help_title $doc]
    help_mark_found
    help_sync
    focus $t
}

proc help_back {} {
    global helpwin
    if {$helpwin(at) <= 0} return
    incr helpwin(at) -1
    lassign [lindex $helpwin(history) $helpwin(at)] doc anchor
    help_show $doc#$anchor 0
}

proc help_next {} {
    global helpwin
    if {$helpwin(at) >= [llength $helpwin(history)] - 1} return
    incr helpwin(at)
    lassign [lindex $helpwin(history) $helpwin(at)] doc anchor
    help_show $doc#$anchor 0
}

# The buttons, and the Contents tree's selection, follow the page shown.
#
# The tree's selection is set only when it differs, and the item set is
# remembered: selecting raises <<TreeviewSelect>> once this has returned, and
# a page shown for that would select its item again, and so on for ever -- a
# window that hung the game the first time a page with an item was opened.
proc help_sync {} {
    global helpwin

    set w .help.f
    $w.tools.back state [expr {$helpwin(at) > 0 ? "!disabled" : "disabled"}]
    $w.tools.next state [expr {$helpwin(at) < [llength $helpwin(history)] - 1
        ? "!disabled" : "disabled"}]

    set tv $w.pw.side.nb.contents.tv
    lassign [lindex $helpwin(history) $helpwin(at)] doc anchor
    set item ""
    foreach candidate [list t:$doc#$anchor t:$doc] {
        if {[$tv exists $candidate]} { set item $candidate ; break }
    }
    set helpwin(selected) $item
    if {$item ne ""} {
        set parent [$tv parent $item]
        while {$parent ne ""} {
            $tv item $parent -open 1
            set parent [$tv parent $parent]
        }
        if {[$tv selection] ne [list $item]} { $tv selection set [list $item] }
        $tv see $item
    } elseif {[llength [$tv selection]]} {
        $tv selection set {}
    }
}

# An item chosen in the Contents tree -- by the player, not by help_sync.
proc help_tree_pick {tv} {
    global helpwin
    set item [lindex [$tv selection] 0]
    if {$item eq $helpwin(selected) || ![string match t:* $item]} return
    help_show [string range $item 2 end]
}

proc help_toggle_contents {} {
    global helpwin

    set pw .help.f.pw
    if {$helpwin(shown)} {
        $pw forget $pw.side
        set helpwin(shown) 0
    } else {
        $pw insert 0 $pw.side -weight 0
        set helpwin(shown) 1
    }
    .help.f.tools.hide configure -text [expr {$helpwin(shown) ? "Hide" : "Show"}]
    .help.mb.about entryconfigure 0 \
        -label [expr {$helpwin(shown) ? "Hide Contents" : "Show Contents"}]
}

# --- searching -------------------------------------------------------------------

# Every page that has the words, most mentions first; the matches are marked on
# the page shown, and Find Again steps through them.
proc help_search {} {
    global helpwin

    set query [string trim $helpwin(query)]
    set lb .help.f.pw.side.nb.search.lb
    $lb delete 0 end
    set helpwin(results) {}
    if {$query eq ""} {
        help_mark_found
        return
    }
    set found {}
    foreach doc [help_docs] {
        set n [regexp -all -nocase -- [help_quote $query] [help_plain $doc]]
        if {$n} { lappend found [list $doc $n] }
    }
    set found [lsort -integer -decreasing -index 1 $found]
    foreach hit $found {
        lassign $hit doc n
        $lb insert end "[help_title $doc]  ($n)"
        lappend helpwin(results) $doc
    }
    set helpwin(status) [expr {[llength $found]
        ? "[llength $found] pages mention \"$query\"."
        : "Nothing in the manual mentions \"$query\"."}]
    help_mark_found
}

proc help_quote {text} {
    return [regsub -all {[][\\.^$*+?(){}|]} $text {\\&}]
}

proc help_result {} {
    global helpwin
    set lb .help.f.pw.side.nb.search.lb
    set i [$lb curselection]
    if {$i eq ""} return
    help_show [lindex $helpwin(results) $i]
    help_find_again
}

proc help_mark_found {} {
    global helpwin

    set t .help.f.pw.page.t
    $t tag remove found 1.0 end
    $t tag remove here 1.0 end
    set helpwin(hits) {}
    set helpwin(hit) -1
    set query [string trim $helpwin(query)]
    if {$query eq ""} return
    set counts {}
    set starts [$t search -all -nocase -count counts -- $query 1.0 end]
    foreach start $starts n $counts {
        set stop [$t index "$start + $n chars"]
        $t tag add found $start $stop
        lappend helpwin(hits) [list $start $stop]
    }
}

proc help_find_again {} {
    global helpwin

    if {![llength $helpwin(hits)]} {
        if {[string trim $helpwin(query)] ne ""} { bell }
        return
    }
    set t .help.f.pw.page.t
    set helpwin(hit) [expr {($helpwin(hit) + 1) % [llength $helpwin(hits)]}]
    lassign [lindex $helpwin(hits) $helpwin(hit)] start stop
    $t tag remove here 1.0 end
    $t tag add here $start $stop
    $t see $start
    set helpwin(status) "Match [expr {$helpwin(hit) + 1}] of [llength $helpwin(hits)]\
        on this page."
}

proc help_find {} {
    set nb .help.f.pw.side.nb
    if {![winfo ismapped $nb]} help_toggle_contents
    $nb select $nb.search
    focus $nb.search.e
    $nb.search.e selection range 0 end
}

# --- the window --------------------------------------------------------------------

proc help_menus {} {
    global helpwin

    set mb .help.mb
    menu $mb
    .help configure -menu $mb

    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about -tearoff 0
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "Hide Contents" -underline 0 \
        -command help_toggle_contents
    $mb.about add command -label "Find…" -underline 0 -command help_find
    $mb.about add command -label "Find Again" -underline 5 -accelerator F3 \
        -command help_find_again
    $mb.about add separator
    $mb.about add command -label "Open in Browser" -underline 0 \
        -command help_browser
    $mb.about add separator
    $mb.about add command -label Close -underline 0 -command {destroy .help}

    # The chapters, as the original's Section menu had them.
    menu $mb.section -tearoff 0
    $mb add cascade -label Section -underline 0 -menu $mb.section
    $mb.section add radiobutton -label "Table of Contents" \
        -variable helpwin(section) -value @contents \
        -command {help_show @contents}
    foreach part [help_contents] {
        $mb.section add separator
        lassign $part kind title target children
        $mb.section add radiobutton -label $title -variable helpwin(section) \
            -value $target -command [list help_show $target]
        foreach chapter $children {
            lassign $chapter kind title target
            if {[string match *#* $target]} continue
            $mb.section add radiobutton -label "    $title" \
                -variable helpwin(section) -value $target \
                -command [list help_show $target]
        }
    }
}

proc help_tree_fill {tv nodes parent} {
    foreach node $nodes {
        lassign $node kind title target children
        set item [$tv insert $parent end -id t:$target -text $title \
            -open [expr {$parent eq ""}]]
        help_tree_fill $tv $children $item
    }
}

# Open the window, at a page if one is named; a window already open is raised
# and left where it was.
proc help_window {{target ""}} {
    global helpwin

    if {[winfo exists .help]} {
        wm deiconify .help
        raise .help
        if {$target ne ""} { help_show $target }
        return
    }

    toplevel .help
    wm title .help "Zangband: Help"
    wm protocol .help WM_DELETE_WINDOW { destroy .help }
    wm geometry .help 1040x760
    wm minsize .help 640 420
    help_menus

    set w .help.f
    ttk::frame $w
    pack $w -fill both -expand 1

    # The original's toolbar: Hide, then Back and Next.  Contents takes the
    # place of its Refresh, which a page that cannot change does not need.
    ttk::frame $w.tools -padding {6 4}
    pack $w.tools -side top -fill x
    ttk::button $w.tools.hide -text Hide -underline 0 -width 8 \
        -command help_toggle_contents
    ttk::separator $w.tools.sep -orient vertical
    ttk::button $w.tools.back -text "◀ Back" -underline 2 -width 8 \
        -command help_back
    ttk::button $w.tools.next -text "Next ▶" -underline 0 -width 8 \
        -command help_next
    ttk::button $w.tools.home -text Contents -underline 0 -width 10 \
        -command {help_show @contents}
    pack $w.tools.hide -side left
    pack $w.tools.sep -side left -fill y -padx 8
    pack $w.tools.back $w.tools.next -side left -padx {0 4}
    pack $w.tools.home -side left -padx {8 0}

    ttk::label $w.status -textvariable helpwin(status) -relief sunken \
        -anchor w -padding {4 1}
    pack $w.status -side bottom -fill x -padx 2 -pady 2

    set pw $w.pw
    ttk::panedwindow $pw -orient horizontal
    pack $pw -fill both -expand 1 -padx 6 -pady {0 4}

    # --- the Contents and Search tabs
    set side $pw.side
    ttk::frame $side
    set nb $side.nb
    ttk::notebook $nb
    pack $nb -fill both -expand 1

    set c [ttk::frame $nb.contents -padding 4]
    $nb add $c -text Contents
    ui_treeview Help.Treeview [uifont 13]
    ttk::treeview $c.tv -show tree -selectmode browse -style Help.Treeview \
        -yscrollcommand [list $c.sb set]
    $c.tv column #0 -width 290
    ttk::scrollbar $c.sb -orient vertical -command [list $c.tv yview]
    pack $c.sb -side right -fill y
    pack $c.tv -side left -fill both -expand 1
    help_tree_fill $c.tv [help_contents] ""
    bind $c.tv <<TreeviewSelect>> { help_tree_pick %W }

    set s [ttk::frame $nb.search -padding 4]
    $nb add $s -text Search
    ttk::label $s.l -text "Words to find:"
    ttk::entry $s.e -textvariable helpwin(query)
    ttk::button $s.go -text Search -command help_search
    set ground [ttk::style lookup . -background]
    listbox $s.lb -selectmode browse -exportselection 0 -activestyle none \
        -background white -relief sunken -borderwidth 2 \
        -highlightthickness 1 -highlightbackground $ground \
        -yscrollcommand [list $s.sb set]
    ttk::scrollbar $s.sb -orient vertical -command [list $s.lb yview]
    grid $s.l -row 0 -column 0 -columnspan 3 -sticky w
    grid $s.e -row 1 -column 0 -columnspan 2 -sticky ew -pady 4
    grid $s.go -row 1 -column 2 -sticky e -padx {4 0}
    grid $s.lb -row 2 -column 0 -columnspan 2 -sticky nsew
    grid $s.sb -row 2 -column 2 -sticky nsw
    grid rowconfigure $s 2 -weight 1
    grid columnconfigure $s 0 -weight 1
    bind $s.e <Return> { help_search ; break }
    bind $s.lb <<ListboxSelect>> help_result

    # --- the page
    set p $pw.page
    ttk::frame $p
    set t $p.t
    text $t -wrap word -font helpbody -background white -foreground black \
        -relief sunken -borderwidth 2 -highlightthickness 0 \
        -padx 4 -pady 8 -cursor arrow -insertwidth 0 -takefocus 1 \
        -spacing1 0 -spacing3 0 -width 70 -height 30 \
        -yscrollcommand [list $p.sy set] -xscrollcommand [list $p.sx set]
    ttk::scrollbar $p.sy -orient vertical -command [list $t yview]
    ttk::scrollbar $p.sx -orient horizontal -command [list $t xview]
    grid $t -row 0 -column 0 -sticky nsew
    grid $p.sy -row 0 -column 1 -sticky ns
    grid $p.sx -row 1 -column 0 -sticky ew
    grid rowconfigure $p 0 -weight 1
    grid columnconfigure $p 0 -weight 1
    help_text_tags $t

    $pw add $side -weight 0
    $pw add $p -weight 1
    set helpwin(shown) 1
    set helpwin(selected) ""
    set helpwin(history) {}
    set helpwin(at) -1
    set helpwin(doc) ""

    # Reading is scrolling: the keys move the page, as they did in the
    # original, and the window keeps them -- a page is no place for a key to
    # set off a command in the game.
    foreach {key how} {
        Up {-1 units} Down {1 units} Prior {-1 pages} Next {1 pages}
        space {1 pages}
    } {
        bind $t <Key-$key> "$t yview scroll $how ; break"
    }
    bind $t <Home> "$t yview moveto 0 ; break"
    bind $t <End> "$t yview moveto 1 ; break"
    bind $t <Button-1> [list focus $t]
    bind .help <Escape> { destroy .help ; break }
    bind .help <F3> { help_find_again ; break }
    accel_bind .help h { help_toggle_contents ; break }
    accel_bind .help b { help_back ; break }
    accel_bind .help n { help_next ; break }
    accel_bind .help c { help_show @contents ; break }
    accel_bind .help f { help_find ; break }
    accel_bind .help a { help_find_again ; break }
    accel_bind .help o { help_browser ; break }

    help_show [expr {$target ne "" ? $target : "@contents"}]
}
