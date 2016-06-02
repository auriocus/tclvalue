lappend auto_path [file dirname [file dirname [info script]]]
package require tclvalue

tclvalue::register preparedstatement {
	# read-only data type. Cache a prepared statement
	variable sql
	variable db
	variable stmt
	
	constructor {SQL} {
		set sql $SQL
		set db {}
		set stmt {}
	}

	destructor {
		my free
	}

	method free {} {
		if {$stmt ne {}} {
			$stmt close
			set stmt {}
		}
	}

	method execute {db_ args} {
		# execute this statement for database db_
		if {($db ne $db_) || ($stmt eq {})} {
			# Prepare statement for this db
			my free
			set db $db_
			set stmt [$db prepare $sql]
		}
		return [$stmt execute {*}$args]
	}

}

tclvalue::register sqlresult {
	variable res
	constructor {resultset} {
		set res $resultset
	}
	
	destructor {
		$res close
	}

	method repr {} {
		# We are forced to create a list/string :(
		return [$res allrows -as lists]
	}

	method iterate {} {
		# nice: we can spit out each value
		yield
		$res foreach -as lists -- row {
			yield $row
		}
		return -code break
	}
}

proc connect {{dbfile test.db}} {
	variable cachedb
	tclvalue::interp eval { package require tdbc::sqlite3 }
	set cachedb [tclvalue::interp eval [list tdbc::sqlite3::connection create db $dbfile]]
}

proc query {db SQL args} {
	set stmt [tclvalue::shimmer $SQL preparedstatement]
	set resultset [$stmt execute $db {*}$args]
	tclvalue::new sqlresult $resultset
}

proc testsql {} {
	set db [connect demo/people.db]
	set q {SELECT * from people}
	set result [query $db $q]
	foreach row $result {
		puts "One set: $row"
	}
	puts [tcl::unsupported::representation $q]
	puts [tcl::unsupported::representation $result]
	puts $result
	puts [tcl::unsupported::representation $result]
}

