#!/usr/bin/tclsh

# Scaler configuration
#    Each item in list represents a Joerger VSC module
#    Each sublist indicates {"base address" nscalers}
set sc(config) {}

set sc(xmin)  2 ;# Low number of bank to be grouped and plotted
set sc(xmax)  8 ;# High number of bank to be grouped and plotted
set sc(extfreq)  10000  ;# External clock frequency
set sc(plotdata) 0     ;# Do not plot scaler data

set comm(control) /dev/ttyViper
set comm(chan)    null
set comm(mode)    "9600,n,8,1"
#set comm(mode)    "4800,o,7,1"
set comm(command) ""
set comm(success) 1
set comm(prompt)  "VIPER> "
set comm(rpcport) 51330

set mc(modcfg)  "$env(HOME)/module.cfg"              ;# Module Configuration
set mc(cfgfile) "$env(HOME)/motor.cfg"               ;# Motor configuration
set mc(posfile) "$env(HOME)/motpos.dat"              ;# Motor positions
set mc(qposfile) "$env(HOME)/qmotpos.dat"           ;# Motor positions (QViper)
set mc(userlog) "$env(HOME)/viper.log"               ;# Log of commands
set mc(config)   {}                                  ;# Indexer Module Base Addresses
set mc(stattime) 1                                   ;# Motor status poll time
set mc(airpad)   0                                   ;# Raise/lower airpads

if [catch {ModCfgRead} result] {
    puts stderr $result
    exit
}

# Read motor configuration from tabular text file
if [catch {MotCfgRead} result] { 
    puts stderr $result
    exit
}
