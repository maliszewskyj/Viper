
#!/usr/bin/tclsh
#
# Version - $Id: scaler.tcl,v 1.22 2010/01/14 14:46:19 nickm Exp $
#
# Scaler geometry - timer      = master board chan 0 (extfreq)
#                   monitor    = master board chan 1
#                   detector 1 = master board chan 2 
#                   total      = sum all boards
#
proc scaler { action args } {
    global sc
    switch $action {
	arm {
	    return [ScalerArm]
	}
	preset {
	    if {[llength $args] < 2} {
		return -code error "Usage: scaler preset <channel> <counts>"
	    }
	    return [ScalerPreset [lindex $args 0] [lindex $args 1]]
	}
	time {
	    if {[llength $args] < 1} {
		return -code error "Usage: scaler time <seconds>"
	    }
	    return [ScalerTime [lindex $args 0]]
	}
	monitor {
	    if {[llength $args] < 1} {
		return -code error "Usage: scaler monitor <counts>"
	    }
	    return [ScalerMon [lindex $args 0]]
	}
	select {
	    if {[llength $args] < 1} {
		return -code error "Usage: scaler select <chan>"
	    }
	    return [ScalerSelect [lindex $args 0]]
	}
	abort {
	    return [ScalerAbort]
	}
	status {
	    return [ScalerStatus]
	}
	counting {
	    return $sc(counting)
	}
	read {
	    return [ScalerRead]
	}
	nchans {
	    return $sc(nscl)
	}
	elapsed {
	    return $sc(elapsed)
	}
	extfreq {
	    return $sc(extfreq)
	}
	reset {
	    return [ScalerReset]
	}
	pause {
	    return [ScalerPause]
	}
	resume {
	    return [ScalerResume]
	}
	default {
	    return -code error "Options: time monitor abort status read"
	}
    }
}

proc ScalerClear { } {
    global sc
    set vector {0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0}
    catch {$sc(master) preload $vector} result
    foreach v $sc(slaves) {
	catch {$v preload $vector} result
    }
}

proc ScalerReset { } {
    global sc

    if {$sc(nscl) > 0} {
	set sc(counts) {}
	set sc(coffset) {}
	for {set i 0} {$i < $sc(nscl)} {incr i} {
	    lappend sc(counts)  0
	    lappend sc(coffset) 0
	}
    }
    set sc(tottime)  0
    set sc(toffset)  0
    set sc(moffset)  0
    set sc(monitor)  0
    set sc(detector) 0
    set sc(total)    0
    set sc(npreset)  0
    set sc(preset)   0
    set sc(elapsed)  0
    set sc(cpreset) -1
    set sc(paused)   0
    ScalerModReset
    ScalerClear
    catch {ScalerPlotData}
    return "Reset Done"
}

proc ScalerModReset { } {
    global sc
    catch {$sc(master) reset} result
    foreach s {$sc(slaves)} { 
	catch {$s reset} result
    }
    return "Modules reset"
}

proc ScalerPreset { chan value } {
    global sc
    # Check to make sure chan is within range
    # This only works for presets
    if { ($chan > 7) || ($chan < 0)} { 
	return -code error "Preset channel out of bounds"
    }
    if { $chan == 0 } {
	set sc(mode) time
	set sc(preset)  [format %.4f [expr $value / $sc(extfreq)]]
    } elseif { $chan == $sc(mctr) } {
	set sc(mode) monitor
	set sc(preset)  $value
    } else {
	set sc(mode) preset
	set sc(preset)  $value
    }

    ScalerClear

    set sc(cpreset) $chan
    set sc(rpreset) [expr int($value - 1)]

    set preload {}
    set dirmask [expr 1 << $chan]
    for {set i 0} {$i < 16} {incr i} {
	set val 0
	if {$i == $chan } {
	    set val $sc(rpreset)
	}
	lappend preload $val
    }
    if [catch {$sc(master) direction $dirmask} result] {
	return -code error $result
    }
    if [catch {$sc(master) preload $preload} result] {
	return -code error $result
    }
    return "Channel $chan preset to $value"
}

proc ScalerArm { } {
    global sc

    # MACS Cruft
    foreach s $sc(slaves) {
	catch {$s arm}
    }

    if [catch {$sc(master) arm} result] {
	return -code error $result
    }
    set sc(start)    [clock seconds]
    set sc(coffset)  $sc(counts)
    set sc(toffset)  $sc(tottime)
    set sc(moffset)  $sc(monitor)
    set sc(elapsed)  0
    set sc(end_id)   after#0
    set sc(counting) 1
    set sc(paused)   0

    # Start poll loop
    set ticks [expr int($sc(poll) * 1000)]
    set sc(poll_id) [after $ticks ScalerProgress]
    return "Counting started"
}

proc ScalerEnd { } {
    global sc

    after cancel $sc(poll_id)
    after cancel $sc(end_id)
    if {$sc(counting)} {
	set sc(counting) 0
	set sc(stop)     [clock seconds]
#	set sc(elapsed) [expr $sc(stop) - $sc(start)]
	if [catch {$sc(master) disarm} result] {
	    return -code error $result
	}
	foreach s $sc(slaves) {
	    catch {$s disarm}
	}

    }
    incr sc(npreset)  
}

proc ScalerPause { } {
    global sc
    if {$sc(counting)} {
	if [catch {$sc(master) disarm} result] {
	    return -code error $result
	}
	set sc(paused) 1
	return "Counting paused"
    }
    return -code error "No count in progress"
}

proc ScalerResume { } {
    global sc
    if {$sc(counting) && $sc(paused)} {
	if [catch {$sc(master) arm} result] {
	    return -code error $result
	}
	set sc(paused) 0
	return "Count resumed"
    }
    return -code error "No count or pause in effect"
}

proc ScalerTime { seconds } {
    global sc

    if [catch {expr $seconds * $sc(extfreq)} ticks] {
	return -code error "Specify integer seconds"
    }

    ScalerPreset 0 $ticks
    return [ScalerArm]
}


proc ScalerMon { counts } {
    global sc
    set sc(mode) monitor

    if [catch {expr int($counts * 1)} counts] {
	return -code error "Specify integer counts"
    }

    catch {ScalerPreset $sc(mctr) $counts}
    return [ScalerArm]
}

# Choose which counter will be the "monitor"
proc ScalerSelect { counter } {
    global sc

    if [catch {expr int($counter * 1)} counter] {
	return -code error "Specify integer channel"
    }

    set maxchan [expr [lindex [lindex $sc(config) 0] 1] - 1]
    if {$counter > $maxchan} {
	return -code error "Select channel between 0 and $maxchan"
    }

    set sc(mctr) [expr $counter + 1]
    return "OK"
}

proc ScalerProgress { } {
    global sc
    set ticks [expr int($sc(poll) * 1000)]
    after cancel $sc(poll_id)
    if {$sc(paused)} {
	set result 1
	set sc(counting) 1
    } elseif [catch {ScalerStatus} result] {
	# Could this be the source of our troubles?
	set sc(counting) 0
	set result 0 
    }
    set sc(stop) [clock seconds]
#    set sc(elapsed) [expr $sc(stop) - $sc(start)]
#    set sc(tottime) [format %.03f [expr $sc(toffset) + $sc(elapsed)]]
    catch {ScalerRead}
    if {$result == 0} {
	return [ScalerEnd]
    }

    set sc(poll_id) [after $ticks ScalerProgress]
}

proc ScalerAbort { } {
    global sc
    
    if [catch {$sc(master) disarm} result] {
	return -code error $result
    }
    foreach s $sc(slaves) {
	catch {$s disarm}
    }
    set sc(counting) 0
    set sc(paused)   0
    set sc(stop) [clock seconds]
    return "Stopped"
}

proc ScalerStatus { } {
    global sc

    if [catch {$sc(master) status} result] {
	return -code error $result
    }
    set sc(counting) $result
    return $result
}

proc ScalerRead { } {
    global sc

    # It's at this point that we rectify the counting preset

    if [catch {$sc(master) read} vals] {
	return -code error $vals
    }
    if {$sc(cpreset) >= 0} {
	set raw [lindex $vals $sc(cpreset)]
	set cooked [expr $sc(rpreset) - $raw]
	set counts [lreplace $vals $sc(cpreset) $sc(cpreset) $cooked]
    } else {
	set counts $vals
    }

    foreach s $sc(slaves) {
	if [catch {$s read} vals] {
	    return -code error $vals
	}
	foreach v $vals {
	    lappend counts $v
	}
    }

    set elapsed [expr ([lindex $counts 0] * 1.0) / $sc(extfreq)]
    set sc(elapsed) [format %.4f $elapsed]
    catch {ScalerUpdate $counts}
    return $counts
}

proc ScalerUpdate { counts } {
    global sc

    set lastcounts $sc(coffset)
    set total 0
    set     sc(counts) [expr [lindex $sc(coffset) 0] + [lindex $counts 0]]
    lappend sc(counts) [expr [lindex $sc(coffset) 1] + [lindex $counts 1]]
    for {set i 2} {$i < [llength $counts]} {incr i} {
	set  cts   [lindex $counts $i]
	incr cts   [lindex $lastcounts $i]
	lappend sc(counts) $cts
	if {($i >= $sc(xmin)) && ($i <= $sc(xmax))} {
	    incr total $cts
	}
    }
    set sc(detector) [expr [lindex $sc(coffset) $sc(dctr)] + [lindex $sc(counts) $sc(dctr)]]
    set sc(total)   $total

    set monitor     [lindex $counts $sc(mctr)]
    set sc(monitor) [expr $sc(moffset) + $monitor]
    set sc(tottime) [format %.03f [expr $sc(toffset) + $sc(elapsed)]]

    # Update plot
    #$sc(graph) element configure e0 -ydata \
	#    [lrange $sc(counts) $sc(xmin) $sc(xmax)]
    catch {ScalerPlotData}
}
