# The Store window.
#
# T7's transfer window: the store's shelves on one side, what it will buy on
# the other, each with a quantity and Buy or Sell, the game's description of
# what is chosen beneath, and gold in the status bar.  It opens by itself when
# the character walks into a store and closes when they walk out, which is what
# the original's did.  The home is the same window with Take and Leave for Buy
# and Sell and no prices.
#
# The game's own store screen is still running in the main window, and its
# keys still work: this is a second way to trade, not a replacement.  What it
# shows is angband_store's, which reads the store screen's own lists and
# prices, and what it does is the commands the store screen pushes once it has
# asked its questions, so the trade itself is the game's.
#
# Built from Tk's own widgets in the classic theme (decision 18): two
# ttk::treeviews, ttk::spinboxes for the quantities, and the rest as the other
# windows have it.

set store(msg) ""
set store(refused) ""
set store(gold) ""
set store(owner) ""
set store(stock) {}
set store(wares) {}
set store(qbuy) 1
set store(qsell) 1
set store(note) ""
set store(home) 0

set store(refresh) {GOLD INVENTORY EQUIPMENT STORECHANGED}

proc store_weight {tenths} {
    return [format "%d.%d lb" [expr {$tenths / 10}] [expr {$tenths % 10}]]
}

# --- opening and closing --------------------------------------------------------

# Whether the character is in a store decides whether the window exists.
proc store_check {} {
    if {[catch {angband_store} here]} { set here {} }
    if {[llength $here]} {
        if {![winfo exists .store]} { store_window }
        store_refresh
    } elseif {[winfo exists .store]} {
        destroy .store
    }
}

# Leaving is the game's: the store screen ends on Escape, as from the keyboard,
# and walking out closes this window.
proc store_leave {} {
    angband_key 0xff1b 0 "\x1b"
}

# --- reading the game ----------------------------------------------------------

proc store_refresh {} {
    global store

    if {![winfo exists .store]} return
    if {[catch {angband_store} here] || ![llength $here]} return
    lassign $here name owner purse home gold
    set w .store.f
    set store(home) $home

    wm title .store "Zangband: $name"
    set store(owner) [expr {$home ? "Your home." : "$owner, who will pay up\
        to $purse gold for any one thing."}]
    set store(gold) "Gold: $gold"

    # A character born with no selling gives rather than sells, which is the
    # game's own word for it ("Give which item?"), and the window says why
    # nothing is paid rather than leaving a column of dashes to explain itself.
    # The note has a line of its own under both tables, there only when it has
    # something to say, so that the two sides stay the same shape.
    if {[catch {angband_option birth_no_selling} nosell]} { set nosell 0 }
    set store(note) ""
    if {!$home} {
        $w.body.sell.act.do configure -text [expr {$nosell ? "Give" : "Sell"}]
        if {$nosell} {
            set store(note) "This character was born with no selling: shops\
                pay nothing, and gold found in the dungeon is worth five times\
                as much."
        }
    }
    if {$store(note) eq ""} {
        pack forget $w.note
    } elseif {[winfo manager $w.note] eq ""} {
        pack $w.note -side bottom -fill x -padx 12 -pady {6 0} -after $w.desc
    }

    set store(stock) [angband_store stock]
    set store(wares) [angband_store wares]
    store_fill $w.body.buy.tv $store(stock) stock
    store_fill $w.body.sell.tv $store(wares) wares
    store_pick buy
    store_pick sell
}

# One table, keeping its selection where the same row is still there.
proc store_fill {tv rows kind} {
    global store

    # With the birth option that stops shops paying, an offer is no offer.
    if {[catch {angband_option birth_no_selling} nosell]} { set nosell 0 }
    set keep [lindex [$tv selection] 0]
    $tv delete [$tv children {}]
    foreach row $rows {
        lassign $row n name number weight price glyph colour flag
        set tags {}
        if {$kind eq "stock" && !$flag} { lappend tags short }
        if {$kind eq "wares" && $flag} { set name "$name (worn)" }
        $tv insert {} end -id r$n -tags $tags -values [list $name \
            [store_weight $weight] \
            [expr {$store(home) ? "" : ($kind eq "wares" && $nosell ? "\u2014" : $price)}]]
    }
    if {$keep ne "" && [$tv exists $keep]} {
        $tv selection set $keep
    } elseif {[llength $rows]} {
        $tv selection set [lindex [$tv children {}] 0]
    }
}

# The chosen row of one side: its quantity's range, its button, and for the
# shelves the game's description of it.
proc store_pick {side} {
    global store

    set w .store.f.body.$side
    set id [lindex [$w.tv selection] 0]
    set rows [expr {$side eq "buy" ? $store(stock) : $store(wares)}]
    set row [expr {$id eq "" ? {} : [lsearch -inline -index 0 $rows [string range $id 1 end]]}]

    if {![llength $row]} {
        $w.act.do state disabled
        $w.act.q configure -to 1
        if {$side eq "buy"} { store_text "" "" }
        return
    }
    lassign $row n name number weight price glyph colour flag
    $w.act.q configure -to $number
    set q [expr {$side eq "buy" ? "qbuy" : "qsell"}]
    if {![string is integer -strict $store($q)] || $store($q) > $number || $store($q) < 1} {
        set store($q) 1
    }
    set ok [expr {$side eq "sell" || $flag}]
    $w.act.do state [expr {$ok ? "!disabled" : "disabled"}]

    if {$side eq "buy"} {
        if {[catch {angband_store lore $n} lore]} { set lore "" }
        store_text $name $lore
    }
}

# A row chosen by hand.  A refusal is about the row it refused, so choosing
# another clears it rather than leaving it under the wrong thing.  And what is
# chosen is described in the Recall pane, as the original's store did -- for
# the table being used, which is the one with the keyboard, so that a refresh
# re-selecting rows on both sides does not swap the pane to the other one.
proc store_select {side} {
    global store

    store_pick $side
    set tv .store.f.body.$side.tv
    set id [lindex [$tv selection] 0]
    if {$store(refused) ni [list "" $side,[string range $id 1 end]]} {
        set store(msg) ""
        set store(refused) ""
    }
    if {$id ne "" && [focus] eq $tv} {
        catch {angband_store recall [expr {$side eq "buy" ? "stock" : "wares"}] \
            [string range $id 1 end]}
    }
}

proc store_text {name text} {
    set t .store.f.desc.t
    $t configure -state normal
    $t delete 1.0 end
    if {$name ne ""} {
        $t insert end [string toupper $name 0 0] head "\n" {} \
            [regsub -all {[ \t]+\n} [string trim $text] "\n"]
    }
    $t configure -state disabled
    $t yview moveto 0
}

# --- the trade -----------------------------------------------------------------

proc store_trade {side {quantity ""}} {
    global store

    set w .store.f.body.$side
    set id [lindex [$w.tv selection] 0]
    if {$id eq ""} return
    # A double-click or Return on a greyed row is the greyed button pressed.
    if {[$w.act.do instate disabled]} { bell; return }
    set n [string range $id 1 end]
    set q [expr {$quantity ne "" ? $quantity
        : $store([expr {$side eq "buy" ? "qbuy" : "qsell"}])}]
    set rows [expr {$side eq "buy" ? $store(stock) : $store(wares)}]
    lassign [lsearch -inline -index 0 $rows $n] - name number

    if {[catch {angband_store $side $n $q} err]} {
        set store(msg) $err
        set store(refused) $side,$n
        return
    }
    set store(refused) ""
    if {[catch {angband_option birth_no_selling} nosell]} { set nosell 0 }
    set store(msg) [dict get {
        buy,0 "Bought" sell,0 "Sold" buy,1 "Took" sell,1 "Left"
    } $side,$store(home)]
    if {$side eq "sell" && !$store(home) && $nosell} { set store(msg) "Gave" }
    # Say how many of how many: a stack's name is the whole stack.
    append store(msg) [expr {$number > 1 ? " $q of $name." : " $name."}]
}

# What the window uses itself stays here: the keys that move about the tables
# and what is typed into a quantity.  Everything else goes to the game, whose
# store screen is still listening.
# F1: the manual on the town's shops, or on the house for the home.
proc store_help {} {
    global store
    help_window [help_first [expr {$store(home) ? "towns#your-house"
        : "dungeon#town-buildings"}] dungeon]
}

proc store_key {w keysym n s a} {
    switch -- [winfo class $w] {
        TSpinbox { return }
        Treeview {
            if {$keysym in {Up Down Prior Next Home End Return}} return
        }
    }
    angband_key $n $s $a
}

# --- the window ----------------------------------------------------------------

proc store_side {parent side title verb} {
    global store

    set f $parent.$side
    ttk::labelframe $f -text $title -padding 6

    set tv $f.tv
    ttk::treeview $tv -style Store.Treeview -show headings -selectmode browse \
        -height 12 -columns {item weight price} \
        -yscrollcommand [list $f.sb set]
    ttk::scrollbar $f.sb -orient vertical -command [list $tv yview]
    foreach {col text anchor width stretch} {
        item Item w 260 1  weight Weight e 70 0  price Price e 70 0
    } {
        $tv heading $col -text $text -anchor $anchor
        $tv column $col -anchor $anchor -width $width -stretch $stretch
    }
    $tv tag configure short -foreground #909090

    ttk::frame $f.act
    ttk::label $f.act.l -text "Quantity:" -font [uifont 12]
    ttk::spinbox $f.act.q -from 1 -to 1 -width 4 -font [uifont Courier 12] \
        -textvariable store([expr {$side eq "buy" ? "qbuy" : "qsell"}])
    ttk::button $f.act.do -text $verb -underline 0 -width 8 \
        -command [list store_trade $side]
    pack $f.act.l -side left
    pack $f.act.q -side left -padx {4 8}
    pack $f.act.do -side left

    # The quantity keeps what is typed into it.
    bindtags $f.act.q [list $f.act.q TSpinbox all]
    bind $f.act.q <Return> [list store_trade $side]

    grid $tv -row 0 -column 0 -sticky nsew
    grid $f.sb -row 0 -column 1 -sticky ns
    grid $f.act -row 1 -column 0 -columnspan 2 -sticky w -pady {8 0}
    grid rowconfigure $f 0 -weight 1
    grid columnconfigure $f 0 -weight 1

    bind $tv <<TreeviewSelect>> [list store_select $side]
    bind $tv <Double-1> "if {\[%W identify region %x %y\] eq {cell}} { store_trade $side 1 }"
    bind $tv <Return> [list store_trade $side]
    return $f
}

proc store_window {} {
    global store

    if {[catch {angband_store} here] || ![llength $here]} return
    lassign $here name owner purse home
    set store(home) $home

    toplevel .store
    wm title .store "Zangband: $name"
    wm protocol .store WM_DELETE_WINDOW store_leave
    wm minsize .store 960 600

    set mb .store.mb
    menu $mb
    .store configure -menu $mb
    menu $mb.file
    $mb add cascade -label File -underline 0 -menu $mb.file
    $mb.file add command -label "Leave the Store" -underline 0 \
        -accelerator Esc -command store_leave
    # Not $mb.help: on macOS Tk treats a menu with that name as the platform's
    # own Help menu and fills it with things this window does not have.
    menu $mb.about
    $mb add cascade -label Help -underline 0 -menu $mb.about
    $mb.about add command -label "On Stores..." -underline 0 -accelerator F1 \
        -command store_help

    set w .store.f
    ttk::frame $w
    pack $w -fill both -expand 1

    set ground [ttk::style lookup . -background]
    set field  [ttk::style lookup TEntry -fieldbackground {} white]
    ui_treeview Store.Treeview [uifont 12] \
                -fieldbackground $field -background $field
    ttk::style configure Store.Treeview.Heading -font [uifont 12 bold]

    ttk::label $w.owner -textvariable store(owner) -font [uifont 12]
    pack $w.owner -anchor w -padx 10 -pady {10 4}

    # --- status, buttons and the rule over them, packed before the tables so
    # that they keep their room when the window is made smaller
    ttk::frame $w.status
    pack $w.status -side bottom -fill x -padx 2 -pady 2
    ttk::label $w.status.msg -textvariable store(msg) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor w -padding {4 1}
    ttk::label $w.status.gold -textvariable store(gold) -font [uifont 12] \
        -relief sunken -borderwidth 1 -anchor center -width -16 -padding {4 1}
    pack $w.status.gold -side right -padx {2 0}
    pack $w.status.msg -side left -fill x -expand 1

    ttk::frame $w.btns
    pack $w.btns -side bottom -fill x -padx 10 -pady 10
    ttk::button $w.btns.leave -text [expr {$home ? "Leave Home" : "Leave the Store"}] \
        -default active -command store_leave
    pack $w.btns.leave -side right

    ttk::separator $w.sep -orient horizontal
    pack $w.sep -side bottom -fill x

    ttk::labelframe $w.desc -text Description -padding 6
    pack $w.desc -side bottom -fill x -padx 10 -pady {6 0}
    text $w.desc.t -height 5 -width 60 -wrap word -font [uifont Courier 12] \
        -background $field -foreground black -relief sunken -borderwidth 2 \
        -highlightthickness 1 -highlightcolor black \
        -highlightbackground $ground -padx 4 -pady 4 -state disabled
    $w.desc.t tag configure head -font [uifont Courier 12 bold]
    # A scrollbar, so that a description longer than five lines says so.
    ttk::scrollbar $w.desc.sb -orient vertical -command [list $w.desc.t yview]
    $w.desc.t configure -yscrollcommand [list $w.desc.sb set]
    pack $w.desc.sb -side right -fill y
    pack $w.desc.t -side left -fill x -expand 1

    # Packed by store_refresh, and only for a character born with no selling.
    ttk::label $w.note -textvariable store(note) -font [uifont 12] \
        -wraplength 900 -justify left

    set body $w.body
    ttk::frame $body
    pack $body -fill both -expand 1 -padx 10 -pady {4 0}
    store_side $body buy [expr {$home ? "In Your Home" : "For Sale"}] \
        [expr {$home ? "Take" : "Buy"}]
    store_side $body sell "Your Pack" [expr {$home ? "Leave" : "Sell"}]
    grid $body.buy  -row 0 -column 0 -sticky nsew -padx {0 12}
    grid $body.sell -row 0 -column 1 -sticky nsew
    grid columnconfigure $body {0 1} -weight 1 -uniform side
    grid rowconfigure $body 0 -weight 1

    bind .store <Key> { store_key %W %K [key_sym %N %k] %s %A }
    bind .store <Escape> { store_leave ; break }
    bind .store <F1> { store_help ; break }
    accel_bind .store b { store_trade buy ; break }
    accel_bind .store t { store_trade buy ; break }
    accel_bind .store s { store_trade sell ; break }
    accel_bind .store g { store_trade sell ; break }
    # The home's Leave, which puts an item in it: Sell's place, and its letter.
    accel_bind .store l { store_trade sell ; break }

    focus $body.buy.tv
}

# Walking into a store and out of it: the game says LEAVE_WORLD and
# ENTER_WORLD for those, and its three store events cannot be listened to for
# more than one visit -- see store_notice in main-tcl.c.
foreach event {LEAVE_WORLD ENTER_WORLD} {
    bind . <<Angband_$event>> {+ store_check }
}
foreach event $store(refresh) {
    bind . <<Angband_$event>> {+ store_refresh }
}
