#!/usr/bin/wish
#
# Version - $Id: gui.tcl,v 1.51 2015/07/10 18:47:45 nickm Exp $
#
option add *font {Helvetica -14 bold} widgetDefault
option add *titlefont {Helvetica -14 bold} widgetDefault

#
# Build scaler status panel
#
proc ScalerStatusBuild { p } {
    global sc

    frame $p.panel -borderwidth 2 -relief ridge
    label $p.panel.armed -text Armed -background DarkRed -relief raised \
	    -borderwidth 2 
    set sc(armed) $p.panel.armed
    label $p.panel.modelab -text "Mode:" 
    label $p.panel.mode -textvariable sc(mode) \
	    -width 12 -relief sunken -background white 
    label $p.panel.preslab -text "Preset:" 
    label $p.panel.preset -textvariable sc(preset) \
	    -width 12 -relief sunken -background white -anchor e
    label $p.panel.nprslab -text "N Presets:" 
    label $p.panel.nprset -textvariable sc(npreset) \
	    -width 12 -relief sunken -background white -anchor e
    label $p.panel.timelab -text "Time:" 
    label $p.panel.time -textvariable sc(tottime) \
	    -width 12 -relief sunken -background white -anchor e
    label $p.panel.monlab  -text "Monitor:" 
    label $p.panel.mon  -textvariable sc(monitor) \
	    -width 12 -relief sunken -background white -anchor e
    label $p.panel.detlab  -text "Det 1:"
    label $p.panel.det  -textvariable sc(detector) \
	    -width 12 -relief sunken -background white -anchor e    
    label $p.panel.totlab  -text "Total:" 
    label $p.panel.total -textvariable sc(total) \
	    -width 12 -relief sunken -background white -anchor e

    grid $p.panel.armed   -               -in $p.panel -sticky ew
    grid $p.panel.modelab $p.panel.mode   -in $p.panel -sticky w
    grid $p.panel.preslab $p.panel.preset -in $p.panel -sticky w
    grid $p.panel.nprslab $p.panel.nprset -in $p.panel -sticky w
    grid $p.panel.timelab $p.panel.time   -in $p.panel -sticky w
    grid $p.panel.monlab  $p.panel.mon    -in $p.panel -sticky w
    grid $p.panel.detlab  $p.panel.det    -in $p.panel -sticky w
    grid $p.panel.totlab  $p.panel.total  -in $p.panel -sticky w

    # Create bindings
    trace variable sc(counting) w ScalerArmLight
    trace variable sc(paused) w ScalerArmPaused

    return $p.panel
}

proc ScalerGraphBuild { p } {
    global sc

    if [info exists sc(usegnuplot)] {
	canvas $p.g
    } else {
	blt::barchart $p.g

	set sc(graph) $p.g
	$p.g element create e0
	set xdata {}
	set ydata {}
	for {set i 1} {$i <= $sc(nscl)} {incr i} {
	    lappend xdata $i
	    lappend ydata 0
	}
	$p.g xaxis configure -title {Detector} -stepsize 10
	if {[info exists sc(xmin)] && [info exists sc(xmax)]} {
	    $p.g xaxis configure -min $sc(xmin) -max $sc(xmax)
	}
	set sc(ymin) 0
	set sc(ymax) 1
	$p.g yaxis configure -title {Counts}
	$p.g element configure e0 -xdata $xdata -ydata $ydata
	$p.g yaxis configure -min $sc(ymin) -max $sc(ymax)
	$p.g legend configure -hide true
    }

    ScalerPlotData
    return $p.g
}

proc ScalerPlotData { } {
    global sc

    if [catch {open $sc(datafile) w} dp] {
	puts "Cannot open $sc(datafile)"
	return
    }
    set npts [llength $sc(counts)]
    for {set i 0} {$i < $npts} {incr i} {
	set xval [lindex $i]
	set yval [lindex $sc(counts) $i]
	puts $dp "$xval\t$yval"
    }

    set tmpcnts [lrange $sc(counts) $sc(xmin) $sc(xmax)]
    set sorted [lsort -integer $tmpcnts]
    #set minimum [lindex $sorted 0]
    set minimum 0
    set maximum [lindex $sorted [expr [llength $sorted] - 1]]
    if {$sc(yauto)} {
	if {$minimum == 0 && $maximum == 0} {
	    set maximum 1
	}
	set sc(ymin) $minimum
	set sc(ymax) $maximum
    }
    close $dp

    if {$sc(plotdata)} {
	if [info exists sc(usegnuplot)] {
	    catch {ScalerGraphPlotter}
	} else {
	    catch {$sc(graph) element configure e0 -ydata $tmpcnts}
	    catch {$sc(graph) yaxis configure -min $sc(ymin) -max $sc(ymax)}
	}
    }
}

proc ScalerGraphPlotter { } {
    global sc

    set gp [open "|gnuplot" r+]
    puts $gp "set term tkcanvas"
    puts $gp "set output '$sc(outfile)'"
    puts $gp "set boxwidth 1.0"
    puts $gp "set nokey"
    puts $gp "set style fill solid 1.0 noborder"
    puts $gp "set style data histogram"
    puts $gp "set xlabel '$sc(xtitle)'"
    # Now actually plot
    set plotstring "plot "
    if {$sc(xauto)} {
	append plotstring "\[\] "	
    } else {
 	append plotstring "\[$sc(xmin):$sc(xmax)\] "
    }
    append plotstring "\[$sc(ymin):$sc(ymax)\] "

    append plotstring "'$sc(datafile)' using 1:2 with boxes fs solid lt 2 lw 2"
    #append plotstring "'$sc(datafile)' using 1:2 with filledcurve lt 2 lw 2"
    #puts $plotstring
    puts $gp $plotstring
    close $gp

    after 200
    uplevel #0 source $sc(outfile)
    uplevel #0 ::gnuplot $sc(graph)
}

proc ScalerGraphReplot { } {
    global sc
    if {!$sc(plotdata)} {
	return
    }
    if {[lsearch [info proc] gnuplot] > -1} {
	if [info exists sc(usegnuplot)] {
	    uplevel #0 ::gnuplot $sc(graph)
	}
    }
}


proc ScalerArmLight { args } {
    global sc

    if {$sc(counting)} {
	$sc(armed) configure -background red
    } else {
	$sc(armed) configure -background DarkRed
    }
}

proc ScalerArmPaused { args } {
    global sc
    if {$sc(paused)} {
	$sc(armed) configure -text Paused
    } else {
	$sc(armed) configure -text Armed
    }
}

proc MotorAxisHide { } {
#    wm withdraw .axis
}

proc MotorAxisGo { } {
    global mc
    set axis $mc(current)

    catch {motor move $axis $mc($axis,destination)} result
}

proc MotorAxisHome { } {
    global mc
    set axis $mc(current)
    catch {motor home $axis positive} result
}

#
# Set current axis position
#
proc MotorAxisSet { } {
    global mc
    set axis $mc(current)
    catch {motor position $axis $mc($axis,destination)} result
    
}

#
# Stop axis motion
#
proc MotorAxisStop { args } {
    global mc
    set axis $mc(current)
    catch {motor stop $axis}
}

proc MotorAxisJog { direction args } {
    global mc
    set axis $mc(current)
    set increment $mc($axis,increment)
    switch -glob $direction {
	negative { set increment [expr -1.0 * $increment]}
    }
    #catch {MotorJog $axis $direction}
    catch {MotorDelta $axis $increment}
}

proc MotorAxisControl { axis args } {
    global display mcgui mc tcl_interactive

    set mc(current) $axis
    set mc(select) $mc(current)
    $mcgui(axn) configure -text "Axis \#$axis"
    $mcgui(nam) configure -textvariable mc($axis,label)
    $mcgui(pos) configure -textvariable mc($axis,position)
    $mcgui(sta) configure -textvariable mc($axis,status)
    $mcgui(dst) configure -textvariable mc($axis,destination)
    $mcgui(inc) configure -textvariable mc($axis,increment)
    MotorAxisEditChange $axis
    # Repack if necessary
    set geom $display(geometry)
    switch $display(current) {
	0 {
	    if [info exists display(scaler)] { 
		if [info exists display(graph)] {
		    pack forget display(graph)
		}
		pack forget $display(scaler) $display(motor)
	    } else {
		pack forget $display(motor)
	    }
	    pack $display(motor) $display(control) -side left -anchor nw \
		    -in $display(base)
	    tkwait visibility $display(control)
	    if { 1 != $tcl_interactive } { wm geometry . $geom }
	}
	1 {
	    if [info exists display(scaler)] { 
		if [info exists display(graph)] {
		    pack forget $display(graph)
		}
		pack forget $display(scaler) $display(motor)
	    } else {
		pack forget $display(motor)
	    }
	    pack $display(motor) $display(control) -side left -anchor nw \
		    -in $display(base)
	    tkwait visibility $display(control)
	    if { 1 != $tcl_interactive } { wm geometry . $geom }
	}
    }
    
}

proc MotorAxisChange { } {
    global mc
    if {[lsearch $mc(defined) $mc(select)] >= 0} {
	MotorAxisControl $mc(select)
    }
}

proc MotorAxisControlHide { } {
    global display

    pack forget $display(control)
    MotorAxisEditHide
    incr display(current) -1
    DisplayCycle
}

proc MotorAxisControlBuild { p } {
    global mcgui mc display
#    set p [toplevel .axis]
#    wm withdraw .axis

    set f [frame $p.mctrl -borderwidth 2 -relief ridge]
    set display(control) $f
    set mcgui(axn) [label $f.axnlab -text "Axis \#1" -anchor w]
    set mcgui(nam) [label $f.namlab -textvariable mc(1,label) -anchor w]
    label $f.axlab  -text "Axis:"        -anchor w
    label $f.poslab -text "Position:"    -anchor w
    label $f.stalab -text "Status:"      -anchor w
    label $f.dstlab -text "Destination:" -anchor w
    label $f.inclab -text "Increment:"   -anchor w
    entry $f.ax     -textvariable mc(select) \
	-relief sunken -background white -width 12
    set mcgui(pos) [label $f.pos -textvariable mc(1,position)\
	    -relief sunken -background white -width 12]
    set mcgui(sta) [label $f.sta -textvariable mc(1,status)\
	    -relief sunken -background white -width 12]
    set mcgui(dst) [entry $f.dst -textvariable mc(1,destination)\
	    -relief sunken -background white -width 12]
    set mcgui(inc) [entry $f.inc -textvariable mc(1,increment) \
	    -relief sunken -background white -width 12]
    button $f.chg  -text CHANGE  -command MotorAxisChange
    button $f.go   -text GO      -command MotorAxisGo
    button $f.stop -text STOP    -command MotorAxisStop
    button $f.set  -text SET     -command MotorAxisSet
    button $f.home -text HOME    -command MotorAxisHome
    button $f.eshw -text EDIT    -command MotorAxisEditShow
    button $f.hide -text DISMISS -command MotorAxisControlHide

    button $f.jogneg -text {-}   -command "MotorAxisJog negative"
    button $f.jogpos -text {+}   -command "MotorAxisJog positive"

    grid $f.axnlab $f.namlab -       -in $f

    if {16 < [llength $mc(defined)]} {
	label $f.axislab  -text "Axis:"        -anchor w
	entry $f.axis     -textvariable mc(select) \
	    -relief sunken -background white -width 12
	#button $f.chg  -text CHANGE  -command MotorAxisChange
	grid $f.axislab  $f.axis     $f.chg  -in $f -sticky ew
    }
    grid $f.poslab $f.pos    $f.go   -in $f -sticky ew
    grid $f.stalab $f.sta    $f.set  -in $f -sticky ew
    grid $f.dstlab $f.dst    $f.stop -in $f -sticky ew
    grid $f.inclab $f.inc    $f.home -in $f -sticky ew
    grid x         x         $f.eshw -in $f -sticky ew
    grid $f.jogneg $f.jogpos $f.hide -in $f -sticky ew

    set mc(current) 1

 #   bind $f.jogneg <ButtonPress>   "MotorAxisJog negative"
 #   bind $f.jogneg <ButtonRelease> "MotorAxisStop"
 #   bind $f.jogpos <ButtonPress>   "MotorAxisJog positive"
 #   bind $f.jogpos <ButtonRelease> "MotorAxisStop"

    MotorAxisEditBuild $f

    return $f
}

proc MotorAxisEditBuild { p } {
    global mc megui display

    set f [frame $p.edit -borderwidth 2 -relief ridge]
    set display(edit) $f
    set megui(axn) [label $f.axnlab -text "Axis \#1"]
    set megui(nam) [entry $f.namlab -textvariable mc(1,label) \
	    -relief sunken -background white]

    label $f.drlab -text "Drive res:"   -anchor w
    set megui(dres) [entry $f.drent -textvariable mc(1,dres) \
	    -relief sunken -background white]
    label $f.erlab -text "Encoder res:" -anchor w
    set megui(eres) [entry $f.erent -textvariable mc(1,eres) \
	    -relief sunken -background white]
    label $f.dblab -text "Deadband:" -anchor w
    set megui(dbnd) [entry $f.dbent -textvariable mc(1,deadband) \
	    -relief sunken -background white]

    label $f.dslab -text "dscale:"      -anchor w
    set megui(dscl) [entry $f.dsent -textvariable mc(1,dscale) \
	    -relief sunken -background white]
    label $f.aslab -text "Acceleration:" -anchor w
    set megui(ascl) [entry $f.asent -textvariable mc(1,ascale) \
	    -relief sunken -background white]
    label $f.bslab -text "Base Velocity:" -anchor w
    set megui(bscl) [entry $f.bsent -textvariable mc(1,bscale) \
	    -relief sunken -background white]
    label $f.vslab -text "Top Velocity:"  -anchor w
    set megui(vscl) [entry $f.vsent -textvariable mc(1,vscale) \
	    -relief sunken -background white]
    
    label $f.amlab -text "Axis:"        -anchor w
    set megui(ae) [radiobutton $f.ae -text "Enable"  -value 1 \
	    -variable mc(1,axis_en) -command MotorEnable ]
    set megui(ad) [radiobutton $f.ad -text "Disable" -value 0 \
	    -variable mc(1,axis_en) -command MotorEnable ]

    label $f.emlab -text "Encoder mode:" -anchor w
    set megui(ee) [radiobutton $f.ee -text "Enable"  -value 1 \
	    -variable mc(1,encmode)]
    set megui(ed) [radiobutton $f.ed -text "Disable" -value 0 \
	    -variable mc(1,encmode)]

    label $f.lmlab -text "Limits:" -anchor w
    set megui(le) [radiobutton $f.le -text "Enable"  -value 1 \
	    -variable mc(1,limit_en)]
    set megui(ld) [radiobutton $f.ld -text "Disable" -value 0 \
	    -variable mc(1,limit_en)]

    label $f.smlab -text "Stall detection:" -anchor w
    set megui(se) [radiobutton $f.se -text "Enable"  -value 1 \
	    -variable mc(1,stalldet_en)]
    set megui(sd) [radiobutton $f.sd -text "Disable" -value 0 \
	    -variable mc(1,stalldet_en)]

    label $f.pmlab -text "Pos Maintenance:" -anchor w
    set megui(pe) [radiobutton $f.pe -text "Enable"  -value 1 \
	    -variable mc(1,pm_en)]
    set megui(pd) [radiobutton $f.pd -text "Disable" -value 0 \
	    -variable mc(1,pm_en)]

    set megui(apply) [button $f.apply   -text "Apply" \
	    -command "MotorConfigLoad 1"]
    button $f.save    -text "Save"    -command MotCfgWrite
    button $f.dismiss -text "Dismiss" -command MotorAxisEditHide

    grid $f.axnlab $f.namlab -
    grid $f.drlab  $f.drent  -           -sticky ew
    grid $f.erlab  $f.erent  -           -sticky ew
    grid $f.dblab  $f.dbent  -           -sticky ew
    grid $f.dslab  $f.dsent  -           -sticky ew
    grid $f.aslab  $f.asent  -           -sticky ew
    grid $f.bslab  $f.bsent  -           -sticky ew
    grid $f.vslab  $f.vsent  -           -sticky ew
    grid $f.amlab  $f.ae     $f.ad       -sticky ew
    grid $f.emlab  $f.ee     $f.ed       -sticky ew
    grid $f.lmlab  $f.le     $f.ld       -sticky ew
    grid $f.smlab  $f.se     $f.sd       -sticky ew
    grid $f.pmlab  $f.pe     $f.pd       -sticky ew
    grid $f.apply  $f.save   $f.dismiss  -sticky ew

    return $f
}

proc MotorAxisEditChange { axis } {
    global mc megui

    $megui(axn) configure -text "Axis \#$axis"
    $megui(nam) configure -textvariable mc($axis,label)

    $megui(dres) configure -textvariable mc($axis,dres) 
    $megui(eres) configure -textvariable mc($axis,eres) 
    $megui(dbnd) configure -textvariable mc($axis,deadband)
    $megui(dscl) configure -textvariable mc($axis,dscale)
    $megui(ascl) configure -textvariable mc($axis,ascale)
    $megui(bscl) configure -textvariable mc($axis,bscale)
    $megui(vscl) configure -textvariable mc($axis,vscale)

    $megui(ae) configure -variable mc(${axis},axis_en)
    $megui(ad) configure -variable mc(${axis},axis_en)
    $megui(ee) configure -variable mc(${axis},encmode)
    $megui(ed) configure -variable mc(${axis},encmode)
    $megui(le) configure -variable mc(${axis},limit_en)
    $megui(ld) configure -variable mc(${axis},limit_en)
    $megui(se) configure -variable mc(${axis},sd_en)
    $megui(sd) configure -variable mc(${axis},sd_en)
    $megui(pe) configure -variable mc(${axis},pm_en)
    $megui(pd) configure -variable mc(${axis},pm_en)
    $megui(apply) configure -command "MotorConfigLoad $axis"

}

proc MotorAxisEditShow { } {
    global display
    grid $display(edit) - -
}

proc MotorAxisEditHide { } {
    global display
    grid forget $display(edit)
}

proc MotorAxisStat { p axis } {
    global mc mcgui
 
    set pf [frame $p.$axis]
    set mc($axis,panel) $pf
    button $pf.control -text $axis -width 2 -command "MotorAxisControl $axis"
    label $pf.desc -textvariable mc($axis,label) -width 18 -anchor w
    label $pf.pos -textvariable mc($axis,position) -width 9 \
	    -relief sunken -background white
    set mcgui($axis,sta) [label $pf.stat -textvariable mc($axis,status)  \
	    -width 10 -relief sunken -background white]
    pack $pf.control $pf.pos $pf.stat $pf.desc -side left -in $pf
    return $pf
} 

proc MotorAxisStatHide { axis } {
    global mc mcgui
    pack forget $mc($axis,panel)
}

proc MotorStatusPanelUpdate { axis } {
    global mc mcgui

    set iscurrent [expr $axis == $mc(current)]

    if {0 == $mc($axis,axis_en)} {
	set mc($axis,status) "disabled"
	set color "grey"
	if {$mc($axis,limstat) & 0x2} {
	    set mc($axis,status) "Lim +(dis)"
	    set color "pink"
	} elseif {$mc($axis,limstat) & 0x1} {
	    set mc($axis,status) "Lim -(dis)"
	    set color "pink"
	} elseif {$mc($axis,homestat)} {
	    set mc($axis,status) "Home (dis)"
	}
    } elseif {$mc($axis,moving)} {
	set mc($axis,status) "Moving"
	set color "green"
    } elseif {$mc($axis,fault)} {
	set mc($axis,status) "Fault"
	set color "yellow"
    } elseif {$mc($axis,limstat) & 0x2} {
	set mc($axis,status) "Limit +"
	set color "red"
    } elseif {$mc($axis,limstat) & 0x1} {
	set mc($axis,status) "Limit -"
	set color "red"
    } elseif {$mc($axis,homestat)} {
	set color "white"
	set mc($axis,status) "HOME"
    } elseif {$mc($axis,slipstat)} {
	set mc($axis,status) "Stall"
	set color "red"
    } else {
	set mc($axis,status) "normal"
	set color "white"
    }

    if [info exists mcgui($axis,sta)] {
	$mcgui($axis,sta) configure -background $color
	if {$iscurrent} { $mcgui(sta) configure -background $color }
    }

    update idletasks
}

proc MotorStatPanel { p } {
    global mc
    
    set pm [frame $p.m -borderwidth 2 -relief ridge]
    foreach m $mc(defined) {
	set pma [MotorAxisStat $pm $m]
	pack $pma -in $pm -side top
    }

    # Set variable trace to indicate motor status in color

    return $pm
}

proc RelayPanel { p } {
    global rl
    set f [frame $p.relay -borderwidth 2 -relief ridge]
    set rowlength 8
    set nchans [expr $rl(nmod) * 32]
    for {set i 0} {$i < $nchans} {incr i} {
	set rl(${i},button) [button $f.b${i} -text $i \
			       -command "RelayToggle $i"]
    }

    for {set i 0} {$i < $nchans} {incr i} {
	set row    [expr $i / $rowlength]
	set column [expr $i % $rowlength]
	#puts "row = $row column = $column"
	grid $rl(${i},button) -row $row -column $column -in $f -sticky nsew
    }

    # Leave packing for someone else
    return $f
}

proc RelayPanelUpdate { } {
    global rl
    foreach r $rl(defined) {
	if {$rl(${r},state)} {
	    $rl(${r},button) configure -background red -activeforeground pink
	} else {
	    $rl(${r},button) configure -background \#ede9e3 -activeforeground \#000000
	}
    }
}

proc DioPanel { p } {
    global io
    set f [frame $p.dio -borderwidth 2 -relief ridge]
    set rowlength 8
    set nchans [expr $io(nmod) * 64]
    # Set button state from mask
    set mask $io(mask)
    
    for {set i 0} {$i < $nchans} {incr i} {
	set row    [expr $i / $rowlength]
	set writeable [expr (0x1 << $row) & $mask]
	if {$writeable} {
	    set state normal
	} else {
	    set state disabled
	}
	set io(${i},button) [button $f.b${i} -text $i \
			       -command "DioToggle $i"\
			       -state $state]
    }

    for {set i 0} {$i < $nchans} {incr i} {
	set row    [expr $i / $rowlength]
	set column [expr $i % $rowlength]
	grid $io(${i},button) -row $row -column $column -in $f -sticky nsew
    }

    # Leave packing for someone else
    return $f
}

proc DioPanelUpdate { } {
    global io
    foreach i $io(defined) {
	if {$io(${i},state)} {
	    $io(${i},button) configure -background red -activeforeground pink
	} else {
	    $io(${i},button) configure -background \#ede9e3 -activeforeground \#000000
	}
    }
}

# Show incoming command and result status
#
proc CommEval { } {
    global comm
    if [catch {uplevel \#0 eval $comm(command)} result ] {
	set comm(success) 0
	set output "ERR:$result\r"
    } else {
	set comm(success) 1
	set output "OK:$result\r"
    }	
}

proc CommStatPanel { p } {
    global display env
    set pf [frame $p.comm]

    set pfl [button $pf.label -text "Command:" -background green -command CommEval]
    set pfc [entry $pf.comm -textvariable comm(command) -width 40 \
	    -relief sunken -background white]
    set pfn [button $pf.next -text "Next" -command DisplayCycle]
    set pfe [button $pf.exit -text "Exit" -command "MotorSave; exit"]
    if {![string match $env(DISPLAY) ":0.0"]} {
        #$pfe configure -state disabled
    }
    set comm(indicator) $pfl
    trace variable comm(status) w CommStatChange
    pack $pfl $pfc -side left -in $pf
    pack $pfe -side right -in $pf
    if {$display(havescaler)} {
	pack $pfn -side right -in $pf
    }
    return $pf
}

proc CommStatChange { args } {
    global comm

    if {$comm(success)} {
	set color green
    } else {
	set color red
    }

    $comm(indicator) configure -background $color
}

proc DisplayCycle { } {
    global display sc tcl_interactive
    incr display(current)
    if {$display(current) >= $display(max)} { set display(current) 0 }
    set geom $display(geometry)
    switch $display(current) {
	0 {
	    pack forget $display(scaler) $display(edit) $display(control) 
	    if [info exists display(graph)] { pack forget $display(graph) }
	    if {$display(haverelay)} { pack forget $display(relay)}
	    if {$display(havedio)} { pack forget $display(dio) }
	    pack $display(motor) $display(scaler) -side left -anchor nw \
		    -in $display(base)
	    tkwait visibility $display(scaler)
	    if { 1 != $tcl_interactive } { wm geometry . $geom }
	}
	1 {
	    pack forget $display(motor) $display(scaler) $display(control)
	    pack $display(scaler) -side left -anchor nw -in $display(base)
	    if {$sc(plotdata)} {
		pack $display(graph)  -side left -expand true -fill both \
		    -anchor nw -in $display(base)
		tkwait visibility $display(graph)
	    }
	    if { 1 != $tcl_interactive } { wm geometry . $geom }
	}
	2 {
	    if [info exists display(graph)] {
		pack forget $display(graph)
	    }
	    pack forget $display(motor) $display(scaler) $display(control) $display(edit)
	    if {$display(haverelay)} {
		pack $display(relay) $display(scaler) \
		    -side left -anchor nw -in $display(base)
	    } 
	    if {$display(havedio)} {
		pack $display(dio) $display(scaler) \
		    -side left -anchor nw -in $display(base)
	    }
	    
	    tkwait visibility $display(scaler)
	    if {1 != $tcl_interactive } { wm geometry . $geom }
	}
    }
}

proc BuildGUI { } {
    global sc rl io display env tcl_interactive

    wm withdraw .
    frame .p 


    if {![string match $sc(master) null]} {
	set display(havescaler) 1
    } else {
	set display(havescaler) 0
    }

    if {[llength $rl(config)] > 0} {
	set display(haverelay) 1
    } else {
	set display(haverelay) 0
    }
    if {[llength $io(config)] > 0} {
	set display(havedio)   1
    } else {
	set display(havedio)   0
    }

    if {$display(havescaler)} {
	set ps [ScalerStatusBuild .p]
	set display(scaler) $ps
    }

    if {$sc(plotdata)} {
	set pg [ScalerGraphBuild .p]
	set display(graph)  $pg
    }

    if {$display(haverelay)} {
	set pr [RelayPanel .p]
	set display(relay) $pr
    }
    if {$display(havedio)} {
	set pi [DioPanel .p]
	set display(dio)   $pi
    }

    set pm [MotorStatPanel .p]
    set pa [MotorAxisControlBuild .p]
    set pc [CommStatPanel .p]

    pack $pc -side bottom -anchor sw -fill x -in .p
    pack $pm -side left -anchor nw -in .p
    if {$display(havescaler)} {
	pack $ps -side left -anchor nw -in .p
    }
    pack .p -expand true -fill both

    set display(base)   .p
    set display(motor)  $pm
    set display(current) 0
    set display(max)     1
    if {$display(havescaler)} { set display(max) 2}
    if {$display(haverelay) || $display(havedio)}  { set display(max) 3}

    set xmax [winfo screenwidth  .]
    set ymax [winfo screenheight .]
    set display(geometry) "${xmax}x${ymax}+0+0"

    # Perhaps set window geometry here
    if { 1 != $tcl_interactive } { 
#	wm overrideredirect . 1
	wm geometry . $display(geometry)
    }

    if {$sc(plotdata)} {
	bind $display(graph) <ResizeRequest> ScalerGraphReplot
	bind $display(graph) <Configure>     ScalerGraphReplot
    }
    # Create additional windows
    wm deiconify .
    return 
}
