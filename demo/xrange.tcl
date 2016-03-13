package require tclvalue

tclvalue::register xrange {
	variable from
	variable to
	constructor {from_ to_} {
		set from $from_
		set to $to_
	}
	
	method repr {} {
		# We are forced to create a list/string
		set result {}
		for {set i $from} {$i<=$to} {incr i} {
			lappend result $i
		}
		return $result
	}

	method iterate {} {
		# nice: we can spit out each value
		yield
		for {set i $from} {$i<=$to} {incr i} {
			yield $i
		}
		return -code break
	}
}


proc xrange {from to} {
	tclvalue::new xrange $from $to
}
