#!/usr/bin/tclsh

proc dio { action args } {
    global io
    switch $action {
	read {
	    return [DioRead]
	}
	stat {
	    if {[llength $args] < 1} {
		return -code error "Usage: dio stat <chan>"
	    }
	    return [DioStat [lindex $args 0]]
	}
	set {
	    if {[llength $args] < 1} {
		return -code error "Usage: dio set <chan>"
	    }
	    return [DioSet [lindex $args 0]]
	}
	toggle {
	    if {[llength $args] < 1} {
		return -code error "Usage: dio toggle <chan>"
	    }
	    return [DioToggle [lindex $args 0]]
	}
	clear {
	    if {[llength $args] < 1} {
		return -code error "Usage: dio clear <chan>"
	    }
	    return [DioClr [lindex $args 0]]
	}
	default {
	    return -code error "Options: read set clear"
	}
    }
}

proc DioRead { } {
    global io
    set idx 0
    set result {}
    foreach mod $io(mods) {
	if [catch {$mod read} dios] {
	    return -code error $dios
	}
	foreach d $dios {
	    set io(${idx},state) $d
	    lappend result $d
	    incr idx
	}
    }
    #catch {DioPanelUpdate}
    return $result
}

proc DioStat { chan } {
    global io

    if {[lsearch $io(defined) $chan] < 0} {
	return -code error "Digital I/O channel number out of range"
    }
    return $io(${chan},state)
}

proc DioSet { chan } {
    global io
    set nio [expr $io(nmod) * 64]
    if [catch {expr $chan / 64} modidx] {
	return -code error "Provide digital I/O channel number"
    }
    if [catch {expr $chan % 64} modoffset] {
	return -code error "Provide digital I/O channel number"
    }
    if {$nio <= $chan} {
	return -code error "Digital I/O channel number out of range"
    }
    set mod [lindex $io(mods) $modidx]

    if [catch {$mod set $modoffset} result] {
	return -code error $result
    }
    set io(${chan},state) 1
    return "Channel $chan set"
}


proc DioClr { chan } {
    global io
    set nio [expr $io(nmod) * 64]
    if [catch {expr $chan / 64} modidx] {
	return -code error "Provide digital I/O channel number"
    }
    if [catch {expr $chan % 64} modoffset] {
	return -code error "Provide digital I/O channel number"
    }
    if {$nio <= $chan} {
	return -code error "Digital I/O channel number out of range"
    }
    set mod [lindex $io(mods) $modidx]

    if [catch {$mod clear $modoffset} result] {
	return -code error $result
    }
    set io(${chan},state) 0
    return "Channel $chan cleared"
}

proc DioToggle { chan } {
    global io
    set nio [expr $io(nmod) * 64]
    if [catch {expr $chan / 64} modidx] {
	return -code error "Provide digital I/O channel number"
    }
    if [catch {expr $chan % 64} modoffset] {
	return -code error "Provide digital I/O channel number"
    }
    if {$nio <= $chan} {
	return -code error "Digital I/O channel number out of range"
    }
    set mod [lindex $io(mods) $modidx]
    if {$io(${chan},state)} {
	catch {DioClr $chan}
    } else {
	catch {DioSet $chan}
    }
    after 100
    catch {DioRead}
    catch {DioPanelUpdate}
}

proc DioStatusPoll { } {
    global io
    set io(stattime) 1.0

    catch {DioRead}
    catch {DioPanelUpdate}
    set io(statid) [after [expr int($io(stattime) * 1000)] DioStatusPoll]
}


