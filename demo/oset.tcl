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

	method remove {key} {
		dict unset intrep $key
	}

	method merge {fromObj} {
		set intrep [dict merge $intrep [$fromObj inspect]]
	}

	method iterate {} {
		yield
		dict for {value _} $intrep {
			yield $value
		}
		return -code break ;#finish iteration
	}
}

proc oset_create {args} {
	tclvalue::new oset $args
}

proc oset_insert {varname value} {
	upvar 1 $varname var
	set oset [tclvalue::unshare var]
	set intRep [tclvalue::shimmer $oset oset]
	$intRep insert $value
	tclvalue::invalidate $oset
	set var $oset
}

proc oset_remove {varname value} {
	upvar 1 $varname var
	set oset [tclvalue::unshare var]
	set intRep [tclvalue::shimmer $oset oset]
	$intRep remove $value
	tclvalue::invalidate $oset
	set var $oset
}

proc oset_merge {value1 value2} {
	tclvalue::shimmer $value1 oset
	set uvalue1 [tclvalue::unshare value1]
	set oset1 [tclvalue::getIntRep $uvalue1]
	tclvalue::shimmer $value2 oset
	set oset2 [tclvalue::getSlaveIntRep $value2]

	$oset1 merge $oset2
	tclvalue::invalidate $uvalue1
	return $uvalue1
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

#test

#testForeach

