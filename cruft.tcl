#/usr/bin/tclsh

proc motor_cruft { } {
    global mc

    puts "Executing motor_cruft"
    # NG3 Presample Cruft
    # catch {$mc(6,motcmd) configure $mc(6,motax) -homeparity 1}

    # NG3 Flight Chamber Cruft
    set mc(73,ssibits) 29
    set mc(78,ssibits) 29
    set mc(87,ssibits) 29

}
