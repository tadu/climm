#! /usr/bin/tclsh

# logmod.tcl -- Operates on unified log files.

proc lpop {list} {
	upvar $list l
	set elem [lindex $l 0]
	set l [lrange $l 1 end]
	return $elem
}

proc lset {list args} {
	foreach item $list {
		upvar [lpop args] elem
		set elem $item
	}
	return $list
}

proc getrec {fd} {
	if {[set line [gets $fd]] == "" && [eof $fd]} {
		return {}
	}
	if {![regexp {^# ([0-9]+)(/-?[0-9]+)? ([^ []*\[[^\]]+\]!)?([^ ]+) [^ ]+ ([^" ]*|"[^"]*")\[[^:]*:([0-9]+)[^ ]*\]( \+[0-9]+)?( \([0-9]+\))?(.*)$} $line intro stamp diff src spec x uin nlin type rest]} {
		puts stderr "Error: Line is not a valid record header."
		puts stderr ">>> $line"
		exit 1
	}
	set type [string trim $type " ()"]
	if {[set nlin [string trim $nlin " +"]] == ""} {
		set nlin 0
	}
	set message {}
	while {[incr nlin -1] >= 0 && ([set line [gets $fd]] != "" || ![eof $fd])} {
		lappend message $line
	}
	return [list $stamp $spec $intro $uin $type $message]
}

proc log_merge {argv} {
	fconfigure stdout -translation binary

	foreach file $argv {
		set FILE([set fd [open $file r]]) $file
		fconfigure $fd -translation binary
		if {[set RECORD($fd) [getrec $fd]] == {}} {
			unset RECORD($fd)
			close $fd
			continue
		}
	}

	while {[array names RECORD] != {}} {
		set past 99999999999999
		foreach fd [array names RECORD] {
			if {[string compare [set stamp [lindex $RECORD($fd) 0]] $past] == -1} {
				set past $stamp
				set next $fd
			}
		}
		lset $RECORD($next) stamp spec intro uin type message
		puts $intro
		if {$message != {}} {
			puts [join $message "\n"]
		}
		if {[set RECORD($next) [getrec $next]] == {}} {
			close $next
			unset RECORD($next)
		} elseif {[string compare [set stamp [lindex $RECORD($next) 0]] $past] == -1} {
			puts stderr "Time wrap in $FILE($next),  $stamp < $past"
		}
		unset next stamp spec intro uin type message
	}
}

proc split_bydate {stamp spec uin} {
	return [string range $stamp 0 5]
}
proc split_bymachine {stamp spec uin} {
	return [lindex [split $spec "@"] 1]
}
proc split_byuin {stamp spec uin} {
	return $uin
}

proc log_split {splitter} {
	fconfigure stdin -translation binary
	while {[lset [getrec stdin] stamp spec intro uin type message] != {}} {
		set file [$splitter $stamp $spec $uin]
		if {![info exists oldfile] || $oldfile != $file} {
			if {[info exists fd]} {
				close $fd
			}
			set fd [open ${file}.log a]
			fconfigure $fd -translation binary
			set oldfile $file
		}
		puts $fd $intro
		if {$message != {}} {
			puts $fd [join $message "\n"]
		}
	}
}

proc log_filter {argv} {
	foreach arg $argv {
		set FILTER($arg) {}
	}
	
	fconfigure stdin -translation binary
	fconfigure stdout -translation binary
	fconfigure [set ofd [open filtered.log a]] -translation binary
	while {[lset [getrec stdin] stamp spec intro uin type message] != {}} {
		if {[info exists FILTER(uin:$uin)] 
		    || [info exists FILTER(type:$type)]} \
		{
			set fd $ofd
		} else {
			set fd stdout
		}
		puts $fd $intro
		if {$message != {}} {
			puts $fd [join $message "\n"]
		}
	}
	close $ofd
}

if {$argv != {}} {
	switch -- [lpop argv] {
		-bydate {
			log_split split_bydate
		}
		-bymachine {
			log_split split_bymachine
		}
		-byuin {
			log_split split_byuin
		}
		-merge {
			log_merge $argv
		}
		-filter {
			log_filter $argv
		}
		default {
			puts stderr "Bad Usage."
			puts stderr "logmod.tcl -bydate < input.log"
			puts stderr "           -bymachine < input.log"
			puts stderr "           -byuin < input.log"
			puts stderr "           -merge input.log1 input.log2 ... > merged.log"
			puts stderr "           -filter uin:nnn < input.log > output.log"
			puts stderr "           -filter type:xxx < input.log > output.log"
		}
	}
}

