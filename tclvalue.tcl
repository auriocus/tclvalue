tclvalue::interp eval {
	# library code: spawn proc
	set cocounter 0
	proc spawn {args} {
		set coname ::Coro[incr cocounter]
		coroutine $coname {*}$args
		return $coname
	}
}

rename foreach __foreach
proc foreach {var list args} {
	if {[llength $var] != 1 || [llength $args] != 1} {
		# only intercept the three args form
		tailcall ::__foreach $var $list {*}$args
	}
	
	set intRep [tclvalue::getSlaveIntRep $list]
	if {$intRep eq {} || [catch {tclvalue::interp eval [list spawn $intRep iterate]} iterator]} {
		# not iterable; std foreach
		tailcall ::__foreach $var $list {*}$args
	}

	upvar 1 $var varlocal
	while true {
		set varlocal [tclvalue::interp eval $iterator]
		uplevel 1 {*}$args
	}
}
