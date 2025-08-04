#!/usr/bin/tclsh
#
# Version - $Id: cfgutil.tcl,v 1.22 2014/01/16 19:31:13 nickm Exp $
#
# Utility functions for reading/saving configuration
#
# set mc(cfgfile) "motor.cfg"

proc MotCfgRead { } {
    global mc rootdir

    if {![file exists $mc(cfgfile)]} {
	set mc(cfgfile) ${rootdir}/motor.cfg
    }

    if [catch {open $mc(cfgfile) r} f] {
	return -code error $f
    }
    while {[gets $f line] != 0} {
	if [eof $f] { break }
	regsub {#.*$} $line {} line
	if { [string length $line] == 0 } { continue } 
	set mlist [eval list $line]
	if {[llength $mlist] < 16} { continue }
	set axis  [lindex $mlist 0]
	set mc(${axis},label)        [lindex $mlist 1]
	set mc(${axis},motmod)       [lindex $mlist 2]
	set mc(${axis},motax)        [lindex $mlist 3]
	set mc(${axis},dres)         [lindex $mlist 4]
        set mc(${axis},encmod)       [lindex $mlist 5]
        set mc(${axis},encax)        [lindex $mlist 6]
	set mc(${axis},eres)         [lindex $mlist 7]
	set mc(${axis},ezero)        [lindex $mlist 8]
        set mc(${axis},edir)         [lindex $mlist 9]
	set mc(${axis},deadband)     [lindex $mlist 10]
	set mc(${axis},axis_en)      [lindex $mlist 11]
	set mc(${axis},encmode)      [lindex $mlist 12]
	set mc(${axis},sd_en)        [lindex $mlist 13]
	set mc(${axis},pm_en)        [lindex $mlist 14]
	set mc(${axis},limit_en)     [lindex $mlist 15]
	set mc(${axis},dscale)       [lindex $mlist 16]
	set mc(${axis},bscale)       [lindex $mlist 17]
	set mc(${axis},vscale)       [lindex $mlist 18]
	set mc(${axis},ascale)       [lindex $mlist 19]
	set mc(${axis},fault)        0
	set mc(${axis},is_servo)     0
	set mc(${axis},enable_high)  1
	set mc(${axis},limit_high)   0
	set mc(${axis},ssibits)     25
	if {!$mc(${axis},encmode) && $mc(${axis},sd_en)} {
	    set mc(${axis},sd_en) 0
	}
	if {!$mc(${axis},encmode) && $mc(${axis},pm_en)} {
	    set mc(${axis},pm_en) 0
	}
	lappend mc(defined) $axis
    }
    close $f
    return "Configuration read"
}

proc MotCfgWrite { } {
    global mc
    if [catch {open $mc(cfgfile) w} f] {
	return -code error $f
    }
    puts $f "#axis label modno modaxis dres emod eaxis eres ezero edir dbnd en em sd pm lm  dscale  bscale  vscale  ascale"

    foreach axis $mc(defined) {
	set label  $mc(${axis},label)
	set motmod $mc(${axis},motmod)
	set motax  $mc(${axis},motax)
	set dres   $mc(${axis},dres)
	set encmod $mc(${axis},encmod)
	set encax  $mc(${axis},encax)
	set eres   $mc(${axis},eres)
	set ezero  $mc(${axis},ezero)
	set edir   $mc(${axis},edir)
	set dbnd   $mc(${axis},deadband)
	set en     $mc(${axis},axis_en)
	set encmode $mc(${axis},encmode)
	set pm     $mc(${axis},pm_en)
	set sd     $mc(${axis},sd_en)
	set lm     $mc(${axis},limit_en)
	set dscale $mc(${axis},dscale)
	set bscale $mc(${axis},bscale)
	set vscale $mc(${axis},vscale)
	set ascale $mc(${axis},ascale)

	set    output [format %-2d $axis]
	append output " \""
	append output [format %-15s $label]
	append output "\" "
	append output "$motmod "
	append output "$motax "
	append output [format "%5d " $dres]
	append output "$encmod "
	append output "$encax "
	append output [format "%5d " $eres]
	append output [format "%7d " $ezero]
	append output [format "%2d " $edir]
	append output [format "%3d " $dbnd]
	append output [format "%2d " $en]
	append output [format "%2d " $encmode]
	append output [format "%2d " $sd]
	append output [format "%2d " $pm]
	append output [format "%2d " $lm]
	append output [format "%9.5f " $dscale]
	append output [format "%7.4f " $bscale]
	append output [format "%7.4f " $vscale]
	append output [format "%7.4f"  $ascale]

	puts $f $output
    }
    close $f
    return "Configuration saved"
}

proc ModCfgRead { } {
    global mc sc rl io rootdir

    if {![file exists $mc(modcfg)]} {
	set mc(cfgfile) ${rootdir}/module.cfg
    }

    if [catch {open $mc(modcfg) r} f] {
	return -code error $f
    }
    while {[gets $f line] != 0} {
	if [eof $f] { break }
	regsub {#.*$} $line {} line
	if { [string length $line] == 0 } { continue } 
	set mlist [eval list $line]
	set num   [lindex $mlist 0]    
	set type  [lindex $mlist 1]
	set model [lindex $mlist 2]
	set rtype [lindex $mlist 3]    
	set resource  [lindex $mlist 4]
	if {$rtype != "vme"} { continue }
	set base $resource
	#set base $resource ;# 
	    #puts "Config: type = $type model = $model base = $base"
        # Skip modules which do not have a VME interface (for now)
	switch $type {
	    scaler {
		switch $model {
		    vsc16 {
			lappend sc(config) [list vsc $base 16]
		    }
		    vsc8 {
			lappend sc(config) [list vsc $base 8]
		    }
		    vs64 {
			lappend sc(config) [list vs64 $base 64]
		    }
		    sis3820 {
			lappend sc(config) [list sis $base 32]
		    }
		    vsdum {
			lappend sc(config) [list vsdummy $base 64]
		    }
		}
	    }
	    motor {
		switch $model {
		    oms58 {
			lappend mc(config) [list oms $base]
		    }
		    maxv {
			lappend mc(config) [list maxv $base]
		    }
		    tip {
			lappend mc(config) [list tip $base]
		    }
		    mdummy {
			lappend mc(config) [list mdummy $base]
		    }
		}
	    }
	    relay {
		switch $model {
		    v2210 {
			lappend rl(config) $base
		    }
		}
	    }
	    dio {
		switch $model {
		    v2510 {
			lappend io(config) [list dgio $base]
		    }
		    ip470 {
			lappend io(config) [list ipio $base]
		    }
		}
	    }
	}
    }
    

    close $f

}
