package require tclvalue

tclvalue::register oset {
	variable intrep
	constructor {list} {
		set intrep {}
		foreach l $list { 
			dict set intrep $l 1
		}
	}

	destructor {
		puts "I'm deleted!"
	}

	method repr {} {
		dict keys $intrep
	}

	method inspect {} {
		return $intrep
	}

	method insert {key} {
		dict set intrep $key 1
	}

	method merge {fromObj} {
		set intrep [dict merge $intrep [$fromObj inspect]]
	}

	method iterate {} {
		yield [info coroutine]
		dict for {value _} $intrep {
			yield $value
		}
		return -code break ;#finish iteration
	}
}

proc oset_create {args} {
	tclvalue::shimmer $args oset
	tclvalue::invalidate $args
	return $args
}

proc oset_insert {varname value} {
	upvar 1 $varname var
	set oset [tclvalue::unshare var]
	set intRep [tclvalue::shimmer $oset oset]
	$intRep insert $value
	tclvalue::invalidate $oset
	set var $oset
}

proc inspect {v} {
	puts [tcl::unsupported::representation $v]
}


proc test {} {
	set test [oset_create a b c a]
	oset_insert test f
	oset_insert test d
	oset_insert test a
	unset test

	set a [oset_create a b c]
	set b $a
	oset_insert b d
	puts $a
	puts $b
}

proc testForeach {} {
	set o [oset_create a b c a]
	puts [tcl::unsupported::representation $o]
	foreach x $o {
		puts $x
	}
	puts [tcl::unsupported::representation $o]
}

test

testForeach

