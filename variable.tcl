#!/usr/bin/tclsh
#
# Version - $Id: variable.tcl,v 1.21 2014/02/12 14:22:00 nickm Exp $
#
# Purpose - Set up global variables
#
# Global arrays
#
# mc - motor       controller
# sc - scaler      controller (counter)
# tc - temperature controller
#

# sc - scaler control 
#
set sc(master)   null ;# Command associated with "master" scaler
set sc(slaves)   {}   ;# List of commands associated with "slave" scalers
set sc(mbase)    0    ;# Master module base address
set sc(sbase)    {}   ;# List of slave module base addresses
set sc(mon)      0    ;# Channel on master for monitor
set sc(mode)     time ;# Counting mode: time, monitor
set sc(counts)   {}   ;# List of detector counts
set sc(coffset)  {}   ;# List of detector count offsets
set sc(mctr)     1    ;# Scaler channel on master board acting as monitor
set sc(dctr)     2    ;# Scaler channel acting as "detector 1"
set sc(monitor)  0    ;# Total number of monitor counts
set sc(detector) 0    ;# Total counts in "Detector 1"
set sc(total)    0    ;# Total counts over all detectors (less monitor)
set sc(counting) 0    ;# Flag to indicate count in progress
set sc(poll)     1    ;# Interval to poll master scaler if counting for monitor
set sc(nmods)    8    ;# Number of modules
set sc(preset)   0    ;# Counting preset - useful for display purposes
set sc(npreset)  0    ;# Number of presets counted
set sc(cpreset) -1    ;# Preset channel
set sc(rpreset)  0    ;# "Raw" preset (under the hood)
set sc(extfreq)  1250 ;# Frequency of clock used as time base
set sc(elapsed)  0    ;# Time elapsed since counting started
set sc(tottime)  0.000;# Total time spent counting
set sc(toffset)  0    ;#
set sc(moffset)  0    ;#
set sc(start)    0    ;# Time counting started
set sc(end_id)   {}   ;# After ID of end of counting
set sc(poll_id)  {}   ;# After ID of scaler polling
set sc(armed)    null ;# Widget indicator name
set sc(graph)    null ;# Name of BLT graph displaying scaler information
set sc(paused)   0
set sc(xdata)    {}
set sc(yauto)    1
set sc(xauto)    0
set sc(xmin)     2
set sc(xmax)     100
set sc(ytitle)   {Counts}
set sc(xtitle)   {Detector}
set sc(datafile) /tmp/viper.dat
set sc(outfile)  /tmp/viper.tk
set sc(plotdata) 1    ;# Plot scaler data (1=yes, 0 = no)

# mc - motor control
#
set mc(mbase)    {}     ;# Motor controller base addresses
set mc(modules)  {}     ;# Motor controller commands
set mc(defined)  {}     ;# List of motor numbers
set mc(nmotors)  0      ;# Number of motors
set mc(apermod)  4      ;# Number of axes per module
set mc(poll)     0.5    ;# Interval to poll controllers if motion in progress
set mc(current)  1      ;# Current "active" axis for manual control
set mc(select)   1      ;# 
set mc(ssibits)  25     ;# Encoder databits
# Placeholders for individual motors
set mc(0,cmd)         null ;# Tcl command associated w/ this axis
set mc(0,motax)       0    ;# Axis number on controller associated w/ this axis
set mc(0,label)       null ;# Axis description
set mc(0,conversion)  4000 ;# Steps/engineering unit
set mc(0,velocity)    1000 ;# Steps/sec
set mc(0,acceleration) 100 ;# Steps/sec/sec
set mc(0,position)     0.0 ;# Current axis position
set mc(0,limstat)      0   ;# Limit status
set mc(0,homestat)     0   ;# Home status
set mc(0,moving)       0   ;# Motion status
set mc(0,status)       {}
# Relays
set rl(nmod)           0   ;# Number of relay modules
set rl(mods)           {}  ;# List of modules
set rl(config)         {}  ;# List of base addresses
set rl(defined)        0   
# Digital I/O
set io(nmod)           0   ;# Number of digital I/O modules
set io(mods)           {}  ;# List of modules
set io(config)         {}  ;# List of base addresses
set io(mask)           0x03 ;# Write mask (should really be part of module cfg)
