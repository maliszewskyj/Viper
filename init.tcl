#!/usr/bin/tclsh
#
# Version - $Id: init.tcl,v 1.28 2015/03/24 20:46:46 nickm Exp $
#

# Initialize master scaler
set sc(nscl) 0
if {[llength $sc(config)] >= 1} {
    set s [lindex $sc(config) 0]
    set cmd  [lindex $s 0]
    set base [lindex $s 1]
    set nscl [lindex $s 2]
    if [catch {$cmd create $base $nscl} sc(master)] {
	puts stderr "Can't open master scaler"
	#exit
    }
    set sc(nscl) $nscl
}

# Initialize slave scalers
if {[llength $sc(config)] > 1} {
    foreach s [lrange $sc(config) 1 end] {
	set cmd  [lindex $s 0]
	set base [lindex $s 1]
	set nscl [lindex $s 2]
	if [catch {$cmd create $base $nscl} slave] {
	    puts stderr "Can't open slave scaler $cmd at address $base"
#	    exit
	}
	lappend sc(slaves) $slave
	incr sc(nscl) $nscl
    }
    catch {unset slave s base nscl}
}

set sc(coffset) {}
for {set i 0} {$i < $sc(nscl)} { incr i} {
    lappend sc(coffset) 0
}
catch {unset i}

# Initialize motor controllers
set mc(modules) {} 
if {[llength $mc(config)] > 0} {
    foreach m $mc(config) {
	set cmd  [lindex $m 0]
	set base [lindex $m 1]
	if [catch {$cmd create $base} module] {
	    puts stderr "Can't open motor controller at address $m"
	    #exit
	}
	catch {$m reset}
	lappend mc(modules) $module
    }
}
catch {unset module}
catch {unset m}

foreach i $mc(defined) {
    set mc($i,motcmd)      [lindex $mc(modules) $mc($i,motmod)]
    set mc($i,enccmd)      [lindex $mc(modules) $mc($i,encmod)]
    set mc($i,status)      normal
    set mc($i,fault)       0
    set mc($i,moving)      0
    set mc($i,limstat)     0
    set mc($i,homestat)    0
    set mc($i,slipstat)    0
    set mc($i,destination) 0.0000
    set mc($i,increment)   1.0000
}
catch {unset i}

# Relay configuration
if {[llength $rl(config)] > 0} {
    set r 0
    foreach m $rl(config) {
	if [catch {rly create $m} module] {
	    puts "Can't open relay controller at address $m"
	}
	# Enable relays
	catch {$module write 0}
	for {set i 0} {$i < 32} {incr i} {
	    set rl(${r},state) 0
	    lappend rl(defined) $r
	    incr r
	}
	catch {$module enable 1}
	lappend rl(mods) $module
	incr rl(nmod)
    }
    catch {unset r i}
}

# Digital I/O configuration
if {[llength $io(config)] > 0} {
    set d 0
    foreach m $io(config) {
	set cmd  [lindex $m 0]
	set base [lindex $m 1]
	if [catch {$cmd create $base $io(mask)} module] {
	    puts "Can't open digital I/O module at address $m"
	}
	for {set i 0} {$i < 64} { incr i} {
	    set io(${d},state) 0
	    lappend io(defined) $d
	    incr d
	}
	lappend io(mods) $module
	incr io(nmod)
    }
    catch {unset d i}
}


#catch {fixtty $comm(control)}

if [catch {open $comm(control) w+} comm(chan)] {
    puts stderr "Can't open serial port $comm(control): $comm(chan)"
} else {
    catch {fconfigure $comm(chan) -blocking 0 -encoding binary -mode $comm(mode)}
}

