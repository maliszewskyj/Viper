#!/usr/bin/tclsh
#
# Version - $Id: motor.tcl,v 1.47 2016/12/05 16:19:32 nickm Exp $
#
#
proc motor { action args } {
    global mc
    set largs [llength $args]
    switch $action {
	move {
	    if {$largs < 2} {
		return -code error \
			"Syntax: motor move <axis> <destination>"
	    }
	    return [MotorMove [lindex $args 0] [lindex $args 1]]
	}
	position {
	    switch $largs {
		1 {
		    return [MotorPosition [lindex $args 0]]
		}
		2 {
		    return [MotorPosition [lindex $args 0] [lindex $args 1]]
		}
		default {
		    return -code error "Syntax: motor position <axis> ?pos?"
		}
	    }
	}
	limits {
	    if {$largs < 1} {
		return -code error \
			"Syntax: motor limits <axis>"
	    }
	    return [MotorLimits [lindex $args 0]]
	}
	motion {
	    if {$largs} {
		if [catch {expr 1 *[lindex $args 0]} ax] {
		    return -code error "Specify axis number"
		}
		if {[lsearch $mc(defined) $ax] < 0} {
		    return -code error "Axis number out of range"
		}
		return $mc($ax,moving)
	    } else {
		# Is any axis moving?
		set moving 0
		foreach i $mc(defined) {
		    set moving [expr $moving | $mc($i,moving)]
		}
		return $moving
	    }
	}
	stop {
	    if {$largs} {
		return [MotorStop [lindex $args 0]]
	    } else {
		return [MotorStop all]
	    }
	}
	delta {
	    if {$largs < 2} {
		return -code error \
			"Syntax: motor delta <axis> <destination>"
	    }
	    return [MotorDelta [lindex $args 0] [lindex $args 1]]
	}
	enable {
	    if {$largs >= 1} {
		return [MotorEnable [lindex $args 0] 1]
	    }
	}
	disable {
	    if {$largs >= 1} {
		return [MotorEnable [lindex $args 0] 0]
	    }
	}
	enabled {
	    if {$largs} {
		if [catch {expr 1 *[lindex $args 0]} ax] {
		    return -code error "Specify axis number"
		}
		if {[lsearch $mc(defined) $ax] < 0} {
		    return -code error "Axis number out of range"
		}
		return $mc($ax,axis_en)
	    } else {
		return -code error "Syntax: motor enabled <axis>"
	    }
	}
	home {
	    if {$largs >= 2} {
		return [MotorHome [lindex $args 0] [lindex $args 1]]
	    }
	}
	athome {
	    if {$largs >= 1} {
		return [MotorAtHome [lindex $args 0]]
	    }
	}
	findlim {
	    if {$largs >= 2} {
		return [MotorFindLim [lindex $args 0] [lindex $args 1]]
	    }
	}
	oscillate {
	    if {$largs >= 3} {
		return [MotorStartOsc [lindex $args 0] [lindex $args 1] [lindex $args 2]]
	    }
	}
	velocity {
	    if {$largs >= 1} {
		return [MotorVelocity [lindex $args 0]]
	    }
	}
	acceleration {
	    if {$largs >= 1} {
		return [MotorAcceleration [lindex $args 0]]
	    }
	}
	save {
	    return [MotorSave]
	}
	restore {
	    return [MotorRestore]
	}
    }
}

proc MotorMove { axis destination } {
    global mc

    if {[lsearch $mc(defined) $axis] < 0} {
	return -code error "Bad axis identifier: $axis"
    }

    if {!$mc($axis,axis_en)} {
	return -code error "Axis disabled"
    }

    # Test to see whether any axis is moving
    #    foreach i $mc(defined) {
    #	if {$mc($i,moving)} {
    #	    return -code error "Motion pending"
    #	}
    #    }
    if {$mc($axis,moving)} {
	return -code error "Motion pending"
    }

    # Test to see whether destination is outside limits
    if [catch {expr $destination * 1.0} result] {
	return -code error "Specify destination as real number"
    }

    catch {Logger "motor move $axis $destination"} ;# Log the move
    catch {$mc($axis,motcmd) clear $mc($axis,motax)}

    # Synchronize motor position with encoder position if necessary
    if {$mc($axis,motcmd) != $mc($axis,enccmd)} {
	catch {MotorPosition $axis}
	catch {$mc($axis,motcmd) position $mc($axis,motax) $mc($axis,position)} result
    }

    # Now move the motor
    if [catch {$mc($axis,motcmd) move $mc($axis,motax) $destination} result] {
	return -code error $result
    }

    # Check to see whether we're moving. If not, resubmit the move
    if { 0 == [MotorIsMoving $axis] } {
	if [catch {$mc($axis,motcmd) move $mc($axis,motax) $destination} result] {
	    return -code error $result
	}
    }


    set mc($axis,destination) $destination
    set mc($axis,moving) 1
    set ticks [expr int($mc(poll) * 1000)]
    after $ticks MotorProgress $axis
    catch {MotorStatusPanelUpdate $axis}
    return "Moving $axis to $destination"
}

proc MotorStartOsc { axis center halfamp } {
    global mc
    
    if {[lsearch $mc(defined) $axis] < 0} {
	return -code error "Bad axis identifier: $axis"
    }

    if {!$mc($axis,axis_en)} {
	return -code error "Axis disabled"
    }

    if {$mc($axis,moving)} {
	return -code error "Motion pending"
    }

    if [catch {expr $center * 1.0} result] {
	return -code error "Specify center as real number"
    }

    if [catch {expr $center * 1.0} result] {
	return -code error "Specify halfamplitude as real number"
    }

    # Now move the motor
    catch {Logger "motor oscillate $axis $center $halfamp"} ;# Log the action
    if [catch {$mc($axis,motcmd) oscillate $mc($axis,motax) $center $halfamp} result] {
	return -code error $result
    }

    set mc($axis,destination) $center
    if [catch {$mc($axis,motcmd) status $mc($axis,motax) moving} result] {
	return -code error "MotorOscillate(moving) $result"
    }
    if {$result} {
	set mc($axis,moving) 1
	set ticks [expr int($mc(poll) * 1000)]
	after $ticks MotorProgress $axis
    }
    catch {MotorStatusPanelUpdate $axis}
    return "Oscillation started"
}

proc MotorStopOsc { axis } {
    global mc

    if {[lsearch $mc(defined) $axis] < 0} {
	return -code error "Bad axis identifier: $axis"
    }

    if {!$mc($axis,axis_en)} {
	return -code error "Axis disabled"
    }

    if {!$mc($axis,moving)} {
	return -code error "No motion pending"
    }
    catch {Logger "motor oscillate stop $axis"}
    catch {$mc($axis,motcmd) kill} result
    return
}

proc MotorDelta { axis increment } {
    global mc

    if {[lsearch $mc(defined) $axis] < 0} {
	return -code error "Bad axis identifier: $axis"
    }

    if {!$mc($axis,axis_en)} {
	return -code error "Axis disabled"
    }

    if {$mc($axis,moving)} {
	return -code error "Motion pending"
    }

    # Test to see whether destination is outside limits
    if [catch {expr $increment * 1.0} result] {
	return -code error "Specify destination as real number"
    }

    catch {Logger "motor delta $axis $increment"}
    catch {$mc($axis,motcmd) clear $mc($axis,motax)}
    # Synchronize motor
    if {$mc($axis,motcmd) != $mc($axis,enccmd)} {
	catch {MotorPosition $axis}
	catch {$mc($axis,motcmd) position $mc($axis,motax) $mc($axis,position)} result
    }

    # Now move the motor
    if [catch {$mc($axis,motcmd) delta $mc($axis,motax) $increment} result] {
	return -code error $result
    }

#    set mc($axis,destination) $destination
    set mc($axis,moving) 1
    set ticks [expr int($mc(poll) * 1000)]
    after $ticks MotorProgress $axis
    catch {MotorStatusPanelUpdate $axis}
    return "Moving $axis to $destination"
}

proc MotorProgress { axis } {
    global mc

    if [catch {MotorIsMoving $axis} result] {
	catch {MotorStatusPanelUpdate $axis}
	return
    }

    if {$mc($axis,moving)} {
	set ticks [expr int($mc(poll) * 1000)]
	after $ticks MotorProgress $axis
	catch {MotorPosition $axis} result
	catch {MotorStatusPanelUpdate $axis}
	return
    } else {
	catch {MotorPosition $axis} result
	catch {instrument_motor_cruft}
    }

    # Test axis status at the end of the move
    MotorEnd $axis
}

proc MotorEnd { axis } {
    global mc

    catch {MotorLimits $axis} result
    catch {MotorSlip $axis} result

    # Get final position of motor from controller
    catch {MotorPosition $axis} result
    catch {MotorStatusPanelUpdate $axis}

    # Dump motor position to disk
    catch {MotorSave} 
    
}

proc MotorJog { axis direction } {
    global mc

    switch -glob $direction {
	positive {  }
	negative {  }
	default {
	    return -code error "Valid directions: positive negative"
	}
    }

    if {!$mc($axis,axis_en)} {
	return -code error "Axis disabled"
    }

    if [catch {$mc($axis,motcmd) jog $mc($axis,motax) $direction} result] {
	puts $result
	return -code error $result
    }

    set mc(moving) 1
    set mc($axis,moving)
    set ticks [expr int($mc(poll) * 1000)]
    after $ticks MotorProgress $axis    
    return "Jogging axis $axis at $mc($axis,velocity)"
}

proc MotorHome { axis direction } {
    global mc
    switch -glob $direction {
	positive { }
	negative { }
	default {
	    return -code error "Valid directions: positive negative"
	}
    }

    if {!$mc($axis,axis_en)} {
	return -code error "Axis disabled"
    }

    catch {Logger "motor home $axis $direction"}
    if [catch {$mc($axis,motcmd) home $mc($axis,motax) $direction} result] {
	puts $result
	return -code error $result
    }

    set mc(moving) 1
    set mc($axis,moving)
    set ticks [expr int($mc(poll) * 1000)]
    after $ticks MotorProgress $axis    
    return "Homing axis $axis"
}

proc MotorFindLim { axis direction } {
    global mc
    switch -glob $direction {
	positive { }
	negative { }
	default {
	    return -code error "Valid directions: positive negative"
	}
    }

    if {!$mc($axis,axis_en)} {
	return -code error "Axis disabled"
    }
    if {$mc($axis,moving)} {
	return -code error "Motion pending"
    }

    # Jog to limit
    catch {Logger "motor findlim $axis $direction"}
    if [catch {$mc($axis,motcmd) jog $mc($axis,motax) $direction} result] {
	catch {Logger "  findlim failed: $result"}
	puts $result
	return -code error $result
    }
    set mc(moving) 1
    set mc($axis,moving) 1
    set ticks [expr int($mc(poll) * 1000)]
    after $ticks MotorProgress $axis    
    return "Finding $direction limit for axis $axis"
}

proc MotorPosition { axis args } {
    global mc

    if {[lsearch $mc(defined) $axis] < 0} {
	return -code error "Bad axis identifier: $axis"
    }

    set largs [llength $args]
    # Test axis 1) integer, 2) in range

    if {$largs == 0} {
	if {$mc($axis,encmode)} {
	    set cmd $mc($axis,enccmd)
	    set ax  $mc($axis,encax)
	} else {
	    set cmd $mc($axis,motcmd)
	    set ax  $mc($axis,motax)
	}
	if [catch {$cmd position $ax} position] {
	    return -code error $position
	}
	set mc($axis,position) [format "%+.4f" $position]
	return $mc($axis,position)
    } else {
	set position [lindex $args 0]
	catch {Logger "motor position $axis $position"} ;# Log the action
	if {$mc($axis,motmod) != $mc($axis,encmod)} {
	    if {$largs <= 1} {
		return "Absolute encoder - not changing position"
	    } else {
		if {[string compare [lindex $args 1] "-force"]} {
		    return "Syntax: motor position ?axis? ?-force?"
		}
	    }
	    # Change zero
	    set dir $mc($axis,edir)
	    set dscale $mc($axis,dscale)
	    set res [expr $mc($axis,dscale) * $mc($axis,eres)]
	    if [catch {$mc($axis,enccmd) position $mc($axis,encax)} result] {
		return -code error $result
	    }
	    if [catch {$mc($axis,enccmd) raw $mc($axis,encax)} raw] {
		return -code error $raw
	    }

	    set cts [expr int($position * $res / $dir)]
	    set zero [expr int($raw - $cts)]
	    set mc($axis,ezero) $zero
	    if [catch {$mc($axis,enccmd) configure $mc($axis,encax) -zero $zero} result] {
		return -code error $result
	    }
	}
	if [catch {$mc($axis,motcmd) position $mc($axis,motax) $position} result] {
	    return -code error $result
	}
	
	set mc($axis,position) [format "%+.4f" $position]
    }
}

proc MotorLimits { axis args } {
    global mc

    if {$mc($axis,limit_en)} {
	if [catch {$mc($axis,motcmd) status $mc($axis,motax) limits} result] {
	    return -code error $result
	}
    } else {
	set result 0 ;# If we are not enabling limits, turn off indication
    }
    set mc($axis,limstat) $result
    return $result
}

proc MotorAtHome { axis args } {
    global mc

    if [catch {$mc($axis,motcmd) status $mc($axis,motax) home} result] {
	return -code error $result
    }
    set mc($axis,homestat) $result
    return $result
}

proc MotorSlip { axis args } {
    global mc
    if [catch {$mc($axis,motcmd) status $mc($axis,motax) slip} result] {
	return -code error $result
    }
    set mc($axis,slipstat) $result
    return $result
}

proc MotorFault { axis args } {
    global mc
    if [catch {$mc($axis,motcmd) status $mc($axis,motax) fault} result] {
	return -code error $result
    }
    set mc($axis,fault) $result
    return $result
}

proc MotorIsMoving { axis } {
    global mc

    if [catch {$mc($axis,motcmd) status $mc($axis,motax) moving} result] {
	return -code error $result
    }
    set mc($axis,moving) $result
    return $result
}

proc MotorStop { axis } {
    global mc

    catch {Logger "motor stop $axis"}
    if [regexp $axis all] {
	foreach i $mc(modules) {
	    catch {$i stop} result
	}
    } else {
	if [catch {$mc($axis,motcmd) stop $mc($axis,motax)} result] {
	    return -code error $result
	}
    }
    # Do NOT flag as stopped. Let the controller stop the motor
    # and indicated that it's stopped once it has come to rest
    #set mc($axis,moving) 0
    catch {MotorStatusPanelUpdate $axis}
    return OK
}

# Save motor configuration parameters to controller
proc MotorConfigLoad { axis } {
    global mc

    #maxv0 debug 1
    set encmode $mc($axis,encmode)
    puts "Configure axis $axis motor"
    if {($mc($axis,encmod) != $mc($axis,motmod)) && $encmode} {
	set encmode 0
    }
    if [catch {$mc($axis,motcmd) configure $mc($axis,motax) \
	    -driveres $mc($axis,dres) \
	    -encres   $mc($axis,eres) \
	    -is_servo $mc($axis,is_servo) \
	    -enable_high $mc($axis,enable_high) \
	    -homeparity $mc($axis,home_parity) \
	    -limitparity $mc($axis,limit_high) \
	    -encmode  $encmode \
	    -posmaintenance $mc($axis,pm_en) \
	    -dscale   $mc($axis,dscale) \
	    -bscale   $mc($axis,bscale) \
	    -vscale   $mc($axis,vscale) \
	    -ascale   $mc($axis,ascale) \
	    -stalldetection $mc($axis,sd_en) \
	    -deadband $mc($axis,deadband) \
	    -enable $mc($axis,axis_en) \
	    -limits $mc($axis,limit_en) 
	} result ] {
	return -code error $result
    }
    #puts $result

    set eres [expr $mc($axis,eres) / $mc($axis,dscale)]
    if {$mc($axis,motmod) != $mc($axis,encmod)} {
        #puts "   -- Configure external encoder"
	if [catch {$mc($axis,enccmd) configure $mc($axis,encax) \
		   -resolution $eres \
		   -databits $mc($axis,ssibits) \
		   -gray 1 \
		   -clockrate 10 \
		   -direction $mc($axis,edir) \
		   -zero $mc($axis,ezero)
            } result ] {
	    return -code error $result
	}
    }
}

# Save motor positions to disk
proc MotorVSave { } {
    global mc

    if [catch {open $mc(posfile) w} f] {
	return -code error $f
    }    

    foreach i $mc(defined) {
	puts $f "set mc($i,position) $mc($i,position)"
    }
    close $f
    return "Positions saved (Viper)"
}

# Save motor positions to disk in QViper format
proc MotorQSave { } {
        global mc

    if [catch {open $mc(qposfile) w} f] {
	return -code error $f
    }

    foreach i $mc(defined) {
	puts $f "$i\t$mc($i,position)"
    }
    close $f
    return "Positions saved (QViper)"
}

proc MotorSave { } {
    catch {MotorVSave}
    catch {MotorQSave}
}

proc MotorRestore { } {
    global mc

    if {![file exists $mc(posfile)]} {
	return
    }

    if [catch {source $mc(posfile)} result] {
	return -code error $result
    }
    catch {Logger "motor restore"}
    foreach i $mc(defined) {
	catch {MotorPosition $i $mc($i,position)}
    }
}

proc MotorQRestore { } {
    global mc
    if {![file exists $mc(qposfile)]} {
	return
    }
    if [catch {open $mc(qposfile) r} f] {
	return -code error $f
    }
    while {[gets $f line]} {
	if [eof $f] { break }
	regsub {\#.*$} $line {} line
	if { [string length $line] == 0 } { continue } 
	set mlist [eval list $line]
	set i     [lindex $mlist 0]
	set pos   [lindex $mlist 1]
	set mc($i,position) $pos    
    }
    close $f

    catch {Logger "motor restore (QViper)"}
    foreach i $mc(defined) {
	catch {MotorPosition $i $mc($i,position)}
    }
}

proc MotorStatusAsk { axis } {
    global mc

    catch {MotorPosition $axis}     ;# Get current position
    if {$mc($axis,axis_en)} {
	catch {MotorFault  $axis}       ;# Get fault status
	catch {MotorSlip $axis}         ;# Get current slip status
    }
    catch {MotorAtHome $axis}       ;# Get current home status
    catch {MotorLimits $axis}       ;# Get current limit status
    catch {MotorStatusPanelUpdate $axis} ;# update displayed information
}

proc MotorStatusPoll { } {
    global mc

    foreach i $mc(defined) {
	MotorStatusAsk $i
#	update ;# Respond to any user input
    }
    catch {instrument_motor_cruft} 
    set mc(statid) [after [expr int($mc(stattime) * 1000)] MotorStatusPoll]
}

proc MotorStatusPollStop { } {
    global mc
    after cancel $mc(statid)
    set mc(statid) -1
}

proc MotorEnable { args } {
    global mc

    if {[llength $args] < 2} {
	set axislist $mc(current)
	set axis $mc(current)
	set state $mc($axis,axis_en)
    } else {
	if [regexp all [lindex $args 0]] {
	    set axislist  $mc(defined)
	} else {
	    set axislist  [lindex $args 0]
	}
	set state [lindex $args 1]
    }
    if {0 == $state} {
	set state 0
	set action disable
    } else {
	set state 1
	set action enable
    }

    foreach axis $axislist {
	if {$mc($axis,moving)} {
	    set result "Cannot $action motor $axis while it is moving"
	    return -code error $result
	}
	set mc($axis,axis_en) $state
	catch {Logger "motor $action $axis"}
	if [catch {$mc($axis,motcmd) $action $mc($axis,motax) axis $state} \
		result] {
	    # Write message to status panel
	    set comm(command) $result
	    return -code error $result
	}
	after 200 ;# Wait for just a jiff to enable/disable
    }

    # Make sure motor has been enabled or not
    foreach axis $axislist {
	if [catch {$mc($axis,motcmd) status $mc($axis,motax) \
		enabled} result] {
	    if [catch {$mc($axis,motcmd) status $mc($axis,motax) \
			   enabled} result] {
		return -code error $result
	    }
	}
	set mc($axis,axis_en) $result
	catch {MotorStatusPanelUpdate $axis}    
	if { $result != $state } {
	    return -code error "Could not change state of axis $axis"
	}
    }

    return $state
}

proc MotorIsEnabled { args } {
    global mc

    if {[llength $args] < 1} {
	set axislist $mc(current)
    } else {
	if [regexp all [lindex $args 0]] {
	    set axislist  $mc(defined)
	} else {
	    set axislist  [lindex $args 0]
	}
    }

    set response {}
    foreach axis $axislist {
	if [catch {$mc($axis,motcmd) status $mc($axis,motax) \
		enabled} result] {
	    return -code error $result
	}
	set mc($axis,axis_en) $result
	lappend response $result
    }
    return $response
}

proc MotorVelocity { args } {
    global mc

    set axis [lindex $args 0]
    if {[lsearch $mc(defined) $axis] < 0} {
	return -code error "Bad axis identifier: $axis"
    }

    set velocity [expr $mc($axis,dscale) * $mc($axis,vscale)]

    return $velocity
}

proc MotorAcceleration { args } {    
    global mc

    set axis [lindex $args 0]
    if {[lsearch $mc(defined) $axis] < 0} {
	return -code error "Bad axis identifier: $axis"
    }

    set acceleration [expr $mc($axis,dscale) * $mc($axis,ascale)]

    return $acceleration
    
}

###############################################################################
# The following code is here to permit us to emulate an ACS MCU-2 controller
#
proc IsACS { in } {
    return [regexp {^[0-9][0-9][A-Z]} $in]
}

#
# xxP
# xxP=yyyy
# xxM
# xxQ
#
#
proc ManageACS { in } {
    global mc
    set ax  [string range $in 0 1]
    set cmd [string range $in 2 2]
    set arg [string range $in 3 end]
    if {[string length $arg] > 0} { 
	set havearg 1
    } else {
	set havearg 0
    }

    if [string match [string range $ax 0 0] 0] {
	set axis [expr [string range $ax 1 1] * 1]
    } else {
	if [catch {expr $ax * 1} axis] {
	    # Should check to see whether axis defined
	    return -code error ${ax}?
	}
    }
    set dres 400  ;# Assume half-stepping
    set conversion [expr $mc($axis,dscale) / $dres]
    switch $cmd {
	P { 
	    # Position enter/examine
	    if {$havearg} {
		set steps [expr int([string range $arg 1 end])]
		set destination [ expr $steps * $conversion ]
		
		if [catch {motor position $axis $destination} result] {
		    return -code error ${ax}?
		}
		# response: xx
		return $ax
	    } else {
		if [catch {motor position $axis} result] {
		    return -code error ${ax}?
		}
		set position [expr int($result / $conversion)]
		return $ax[format "P=%+d" $position]
		# response: xxP=+yyyy
	    }
	}
	G { 
	    # Move
	    if {$havearg} {
		# response: xx
		set steps [expr int([string range $arg 0 end])]
		set destination [ expr $steps * $conversion ]
		if [catch {motor move $axis $destination} result] {
		    return -code error ${ax}?
		}
		return $ax
	    } else {
		# response: xx?
		return -code error ${ax}?
	    }
	}
	M {
	    # Motion status
	    if {$havearg} {
		# response: xx?
		return -code error ${ax}?
	    } else {
		# response: xxM=yy
		if [catch {motor motion $axis} result] {
		    return -code error ${ax}?
		}
		catch {expr $result * 1} result
		return $ax[format "M=%02d" $result]
	    }
	}
	E {
	    # Limit status
	    if {$havearg} {
		return -code error ${ax}?
	    } else {
		set pos 0
		set neg 0
		if {$mc($axis,limstat) > 0} {
		    set pos +
		} elseif {$mc($axis,limstat) < 0} {
		    set neg -
		}
		if {$mc($axis,moving)} {
		    set moving S
		} else {
		    set moving 0
		}
		return ${ax}E=${neg}0${pos}A${moving}
	    }
	}
	Q {
	    # Quit
	    if [catch {motor stop $axis}] { return -code error ${ax}? }
	    # response: xx
	    return $ax
	}
	W {
	    if {$havearg} {
		set state [expr int([string range $arg 1 end])]
		switch $state {
		    1 {
			set motorarg "enable"
		    }
		    default {
			set motorarg "disable"
		    }
		}
		if [catch {motor $motorarg $axis}] { return -code error ${ax}? }
		return $ax
	    } else {
		if {$mc($axis,axis_en)} {
		    set motorarg 1
		} else {
		    set motorarg 0
		}
		return ${ax}W=$motorarg
	    }
	}
	L {
	    if {$havearg} {
		switch -- $arg {
		    + {
			if [catch {MotorFindLim $axis positive}] {
			    return -code error ${ax}?
			}
		    }
		    - {
			if [catch {MotorFindLim $axis negative}] {
			    return -code error ${ax}?
			}
		    }
		}
	    } else {
		return -code error ${ax}?
	    }
	    return $ax
	}
	default {
	    return -code error ${ax}?
	}
    }
    return $ax
}
