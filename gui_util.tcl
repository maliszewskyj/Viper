#!/usr/bin/wish
#
#  Version - $Id: gui_util.tcl,v 1.1 2000/02/03 17:57:34 nickm Exp $
#
option add *Notebook.borderWidth 2 widgetDefault
option add *Notebook.relief sunken widgetDefault

proc notebook_create { win } {
    global nbInfo

    frame $win -class Notebook

    pack propagate $win 0

    set nbInfo($win-count)   0
    set nbInfo($win-pages)   ""
    set nbInfo($win-current) ""

    # Remember to bind to destroy method
    bind $win <Destroy> "notebook_destroy $win"

    return $win
}

proc notebook_destroy { win } {
    global nbInfo

    foreach p [array names nbInfo] {
	if [regexp "^$win" $p] {
	    unset nbInfo($p)
	}
    }
}

proc notebook_page { win name } {
    global nbInfo

    set page "$win.page[incr nbInfo($win-count)]"
    lappend nbInfo($win-pages)  $page
    set nbInfo($win-page-$name) $page

    frame $page

    if {$nbInfo($win-count) == 1} {
	after idle [list notebook_display $win $name]
    }
    
    return $page
}

proc notebook_display {win name} {
    global nbInfo

    set page ""
    if {[info exists nbInfo($win-page-$name)]} {
	set page $nbInfo($win-page-$name)
    } elseif {[winfo exists $win.page$name]} {
	set page $win.page$name
    }
    if {"" == $page} {
	error "bad notebook page \"$name\""
    }
    
    # perform size calculation
    notebook_fix_size $win
    if {"" != $nbInfo($win-current)} {
	pack forget $nbInfo($win-current)
    }
    pack $page -expand yes -fill both
    set nbInfo($win-current) $page
    return $page
}

proc notebook_fix_size { win } {
    global nbInfo

    update idletasks

    set maxw 0
    set maxh 0
    foreach page $nbInfo($win-pages) {
	set w [winfo reqwidth $page]
	if {$w > $maxw} {set maxw $w}

	set h [winfo reqheight $page] 
	if {$h > $maxh} {set maxh $h} 
    }

    set bd [$win cget -borderwidth]
    set maxw [expr $maxw + 2 * $bd]
    set maxh [expr $maxh + 2 * $bd]
    $win configure -width $maxw -height $maxh
}

proc toggle_create { win boolvar {boolvals {OFF ON}}} {
    global tInfo

    upvar $boolvar b
    if {[llength $boolvals] != 2} {
	return -code error "toggle_create: specify only two possible labels"
    }

    if {$b} {
	set initvar [lindex $boolvals 1]
    } else {
	set initvar [lindex $boolvals 0]
    }
    set tInfo($win-var) $boolvar
    set tInfo($win-lab) $boolvals
    button $win -text $initvar -command "toggle_action $win"

    bind $win <Destroy> "toggle_destroy $win"

    return $win
}

proc toggle_destroy { win } {
    global tInfo
    unset tInfo($win-var)
    unset tInfo($win-lab)
}

proc toggle_action { win } {
    global tInfo
    upvar $tInfo($win-var) t
    if {$t} {
	$win configure -text [lindex $tInfo($win-lab) 0]
	set t 0
    } else {
	$win configure -text [lindex $tInfo($win-lab) 1]
	set t 1
    }
    return $t
}
