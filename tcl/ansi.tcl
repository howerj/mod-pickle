
proc ansi {cmd args} {
	set c1 ""
	set c2 ""
	if {ne $args ""}      { set c1 [lindex $args 0] }
	if {ne $args ""}      { set c2 [lindex $args 1] }
	if {eq $cmd color}    { return [color $args] }
	if {eq $cmd sgr}      { return "\x1b\[${c1}m" }
	if {eq $cmd up}       { return "\x1b\[${c1}A" }
	if {eq $cmd down}     { return "\x1b\[${c1}B" }
	if {eq $cmd forward}  { return "\x1b\[${c1}C" }
	if {eq $cmd back}     { return "\x1b\[${c1}D" }
	if {eq $cmd next}     { return "\x1b\[${c1}E" }
	if {eq $cmd previous} { return "\x1b\[${c1}F" }
	if {eq $cmd position} { return "\x1b\[${c1};${c2}H" }
	if {eq $cmd clear}    { return "\x1b\[${c1}J" }
	if {eq $cmd status}   { return "\x1b\[6n" }
	return "Invalid subcommand $cmd $args"
}


