#!/usr/bin/tclsh

proc relay { action args } {
    global rl
    switch $action {
	read {
	    return [RelayRead]
	}
	set {
	    if {[llength $args] < 1} {
		return -code error "Usage: relay set <chan>"
	    }
	    return [RelaySet [lindex $args 0]]
	}
	toggle {
	    if {[llength $args] < 1} {
		return -code error "Usage: relay toggle <chan>"
	    }
	    return [RelayToggle [lindex $args 0]]
	}
	clear {
	    if {[llength $args] < 1} {
		return -code error "Usage: relay clear <chan>"
	    }
	    return [RelayClr [lindex $args 0]]
	}
	default {
	    return -code error "Options: read set clear"
	}
    }
}


proc RelayRead { } {
    global rl
    set r_idx 0
    set result {}
    foreach mod $rl(mods) {
	if [catch {$mod read} rlys] {
	    return -code error $rlys
	}
	foreach rly $rlys {
	    set rl($r_idx,state) $rly
	    lappend result $rly
	    incr r_idx
	}
    }
    #catch {RelayPanelUpdate}
    return $result
}

proc RelaySet { rly } {
    global rl
    set nrl [expr $rl(nmod) * 32]
    if [catch {expr $rly / 32} modidx] {
	return -code error "Provide relay number"
    }
    if [catch {expr $rly % 32} modoffset] {
	return -code error "Provide relay number"
    }
    if {$nrl <= $rly} {
	return -code error "Relay number out of range"
    }
    set mod [lindex $rl(mods) $modidx]

    if [catch {$mod set $modoffset} result] {
	return -code error $result
    }
    set rl($rly,state) 1
    return "Relay $rly set"
}

proc RelayClr { rly } {
    global rl
    set nrl [expr $rl(nmod) * 32]
    if [catch {expr $rly / 32} modidx] {
	return -code error "Provide relay number"
    }
    if [catch {expr $rly % 32} modoffset] {
	return -code error "Provide relay number"
    }
    if {$nrl <= $rly} {
	return -code error "Relay number out of range"
    }
    set mod [lindex $rl(mods) $modidx]

    if [catch {$mod clear $modoffset} result] {
	return -code error $result
    }
    set rl($rly,state) 0
    return "Relay $rly cleared"
}

proc RelayToggle { rly } {
    global rl
    set nrl [expr $rl(nmod) * 32]
    if [catch {expr $rly / 32} modidx] {
	return -code error "Provide relay number"
    }
    if [catch {expr $rly % 32} modoffset] {
	return -code error "Provide relay number"
    }
    if {$nrl <= $rly} {
	return -code error "Relay number out of range"
    }
    set mod [lindex $rl(mods) $modidx]
    if {$rl(${rly},state)} {
	catch {RelayClr $rly}
    } else {
	catch {RelaySet $rly}
    }
    after 100
    catch {RelayRead}
    catch {RelayPanelUpdate}
}

proc RelayStatusPoll { } {
    global rl
    set rl(stattime) 1.0

    catch {RelayRead}
    catch {RelayPanelUpdate}
    set rl(statid) [after [expr int($rl(stattime) * 1000)] RelayStatusPoll]
}

