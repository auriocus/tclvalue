package require tclvalue

tclvalue::register asciifile {
	variable fd
	variable filename
	
	constructor {fn} {
		set filename $fn
		set fd [open $filename r]
	}
	
	destructor {
		if {$fd ne {}} { catch {close $fd} }
		set fd {}
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

	method <cloned> {args} {
		return -code error "Can't copy file content"
	}
}


proc readln {fn} {
	tclvalue::new asciifile $fn
}
	
proc testfile {{fn {demo/readfile.tcl}}} {
	proc inspect {v} {
		puts [tcl::unsupported::representation $v]
	}

	set lines [readln $fn]
	foreach line $lines {
		puts $line
	}
	
	inspect $lines
	puts $lines
	inspect $lines
}

