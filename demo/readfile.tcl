package require tclvalue

proc inspect {v} {
	puts [tcl::unsupported::representation $v]
}

tclvalue::register asciifile {
	variable fd
	variable filename
	constructor {args} {
		set fd {}
		set filename {}
	}

	destructor {
		my closeifopen
	}

	method closeifopen {} {
		if {$fd ne {}} { catch {close $fd} }
		set fd {}
	}

	
	method openfile {fn} {
		set filename $fn
		my closeifopen
		set fd [open $filename r]
	}
	

	method repr {} {
		# We are forced to read the whole file
		# into memory. 
		if {$fd eq {}} {
			return "(file not open)"

		}
		seek $fd 0
		split [read $fd] \n
	}

	method iterate {} {
		# nice: we can read line by line
		seek $fd 0
		yield
		while {[gets $fd line]>=0} {
			yield $line
		}
		return -code break
	}
}


proc readln {fn} {
	set dummy "unimportant"
	set myfile [tclvalue::unshare dummy]
	set intRep [tclvalue::shimmer $myfile asciifile]
	$intRep openfile $fn
	tclvalue::invalidate $myfile
	return $myfile
}
	
proc testfile {{fn Makefile}} {
	set lines [readln $fn]
	foreach line $lines {
		puts $line
	}
	
	inspect $lines
	puts $lines
	inspect $lines
}

