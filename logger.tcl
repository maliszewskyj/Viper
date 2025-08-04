#!/usr/bin/tclsh
proc FormatTime { Timeval } {
    return [clock format $Timeval -format "%d-%b-%Y %H:%M:%S"]
}

proc Logger { msg } {
    global mc
    set curtime [FormatTime [clock seconds]]
    if [catch {open $mc(userlog) a} fd] {
	return
    }
    puts -nonewline $fd " $curtime : "
    puts $fd $msg
    close $fd
}
