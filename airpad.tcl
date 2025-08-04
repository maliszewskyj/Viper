#!/usr/bin/tclsh

set comm(rport)    /dev/ttyS0
if {![info exists comm(rchan)]} {
    set comm(rchan)    null
}
set comm(rmode)    "9600,n,8,1"

proc AirPadButton { p } {
    global mc air

    if {![info exists mc(airpad)]} {
	return
    }
    if {$mc(airpad) == 0} {
	set text "Air Manu"
	set command AirPadActivate
    } else {
	set text "Air Auto"
	set command AirPadDeactivate
    }
    set air(button) [button $p.air -text $text -command $command]
    pack $air(button) -side right -in $p
}

proc AirPadActivate { } {
    global mc air
    if {![info exists mc(airpad)]} {
	return
    }
    set mc(airpad) 1
    set text "Air Auto"
    set command AirPadDeactivate
    $air(button) configure -text $text -command $command
}

proc AirPadDeactivate { } {
    global mc air
    if {![info exists mc(airpad)]} {
	return
    }
    set mc(airpad) 0
    set text "Air Manu"
    set command AirPadActivate
    $air(button) configure -text $text -command $command
}

proc AirPadOpenCom { } {
    global comm
    if [catch {open $comm(rport) r+} comm(rchan)] {
	puts stderr "Can't open serial port $comm(rport): $comm(rchan)"
	return -code error "Can't open serial port $comm(rport): $comm(rchan)"
    }
    fconfigure $comm(rchan) -mode $comm(rmode) -blocking 0 -encoding binary
}

proc AirPadCloseCom { } {
    global comm
    close $comm(rchan)
    set comm(rchan) null
    return
}

proc SendMsg { msg } {
    global comm

    puts -nonewline $comm(rchan) "$msg\r"
    flush $comm(rchan)
    #puts "Send: $msg"
    after 100
    gets $comm(rchan) result
    #puts "Recv: $result"
    return $result
}


#
# Use ADAM 4060 relay module to control airpad system
# Turn on the relay to raise the analyzer for movement
# Turn off the relay to lower the analyzer
#

proc AirPadRelayOn { {which 0} } {
    set mask [expr 1 << $which]
    set msg [format "\#0100%02x" $mask] 
    if [catch {SendMsg $msg} result] {
	return -code error $result
    }
}

proc AirPadRelayOff { } {
    catch {SendMsg "\#010000"} result
}

proc AirPadRelayStatus { } {
    catch {SendMsg "\$016"} result
    return $result
}

proc AirPadUp { {axis 11} } {
    global mc comm

    if {![info exists mc(airpad)]} {
	return
    }
    if [string match $comm(rchan) null] {
	if [catch {AirPadOpenCom} result] {
	    return
	}
    }
    if {![expr ($axis == 11) || ($axis == 14) || ($axis == 15)]} {
	return
    }
    if [catch {AirPadRelayOn} result] {
	return "Could not command airpad system up: $result" 
    } 
    # Wait for analyzer to rise
    after 2000 
    set comm(command) "AirPad UP"
    return "AirPad system commanded up"
}

proc AirPadDn { } {
    global mc comm

    if {![info exists mc(airpad)]} {
	return 
    } elseif {$mc(airpad) == 0} {
	return
    }
    # Make sure motor 11 (drum), motor 14 (scattering angle),
    # and motor 15 (sample table trans) are not moving.
    if [string match $comm(rchan) null] {
	if [catch {AirPadOpenCom} result] {
	    return
	}
    }
    if [expr ! ( $mc(11,moving) || $mc(14,moving) || $mc(15,moving) )] {
	AirPadRelayOff
	set comm(command) "AirPad DOWN"
	return "AirPad system commanded down"
    }
}
