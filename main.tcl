#!/usr/bin/wish
#
# Version - $Id: main.tcl,v 1.28 2015/06/13 18:59:10 nickm Exp $
#
# Temp turn on appdebug
set appdebug 1
if {[lsearch $argv "-debug"] != -1} { set appdebug 1 }

if [info exists env(BTXROOT)] {
    set rootdir $env(BTXROOT)
} else {
    set rootdir [pwd]
}

# Load shared libraries and packages
#package require BLT
load ${rootdir}/libbtxvme.so
set log_file "viper_err.log"
set userlog  "viper.log"
set lock_file $::env(HOME)/QViper.lock

# Source dependent scripts
set version {$Id: main.tcl,v 1.28 2015/06/13 18:59:10 nickm Exp $}
source ${rootdir}/variable.tcl   ;# Variable declarations
source ${rootdir}/cfgutil.tcl    ;# Configuration utility functions
source ${rootdir}/config.tcl     ;# Local configuration
source ${rootdir}/init.tcl       ;# Initialize crate configuration
source ${rootdir}/scaler.tcl     ;# Scaler control methods
source ${rootdir}/motor.tcl      ;# Motor control methods
source ${rootdir}/relay.tcl      ;# Relay control methods
source ${rootdir}/dio.tcl        ;# Digital I/O control methods
source ${rootdir}/rpc.tcl        ;# Network command interface
source ${rootdir}/gui_util.tcl   ;# GUI utility functions
source ${rootdir}/gui.tcl        ;# GUI creation functions
source ${rootdir}/logger.tcl
catch {source $env(HOME)/system.cfg} ;# Special bits 

# register background error handler
proc bgerror { arg } {
   global log_file
   set close_log_file 1
   if [catch {open $log_file a} fileId] {
      set fileId stderr
      set close_log_file 0
   }
   puts $fileId \
      "Background Error: $arg | [clock format [clock seconds]]."
   if $close_log_file {
      close $fileId
   }
}

proc acquire_lock { lock_file } {
    if {[catch {$lock_file {CREAT WRONLY EXCL}} fh]} {
	# If 'open' fails, the flie likely exists (lock is held)
	return ""
    } else {
	# If 'open' succeeds, we have the lock. Write the PID, application, 
	puts $fh [pid]
	puts $fh "viper"
	puts $fh [hostname]
	puts $fh "blahblahblah"
	puts $fh "halbhalbhalb"
	close $fh; # Close it immediately to allow others to read/delete
	return $lock_file
    }
}

proc release_lock { lock_file } {
    if {[file exists $lock_file]} {
	file delete $lock_file
	puts "Lock released."
    }
}

proc Exit_Gracefully {} {
    global lock_file
    catch {release_lock $lock_file}
    catch {MotorSave}
    exit
}

# Make sure we start in a pristine state
catch {scaler reset}

# Special system configuration
catch {config_cruft}

# Initialize motor display
foreach i $mc(defined) { 
    catch {MotorConfigLoad $i}
    motor position $i
    motor limits $i
}

catch {setup_cruft}

# Restore motor positions
catch {Logger "VIPER started"}
#catch {MotorRestore}
catch {MotorQRestore}

# Build GUI
catch {BuildGUI}

# Establish fileevent handler
fileevent $comm(chan) readable {
    
    if {[gets $comm(chan) comm(command)] > 0} {
	if {$appdebug} { puts "COMMAND: $comm(command)" }
	if [IsACS $comm(command)] {
	    catch {ManageACS $comm(command)} output
	    append output \r
	} else {
	    if [catch {eval $comm(command)} result] {
		set comm(success) 0
#		set output "ERR:$result\r"
		set output "ERR:$result@$comm(command)\r"
	    } else {
		set comm(success) 1
#		set output "OK:$result\r"
		set output "OK:$result@$comm(command)\r"
	    }
	}
	if {$appdebug} { puts "REPLY  : $output" }
	puts -nonewline $comm(chan) $output
	flush $comm(chan)
    }
}

if [catch {RPCServer $comm(rpcport)} result] {
    puts stderr $result
}

# Start displaying motor status information
MotorStatusPoll
if {[llength $rl(config)] > 0} {
    RelayStatusPoll
}
if {[llength $io(config)] > 0} {
    DioStatusPoll
}

set version

