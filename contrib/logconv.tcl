#! /usr/bin/tclsh

# logconv.tcl -- Converts old micq log files to new unified format.

if {[set servtype [lindex $argv 0]] == "" 
    || [set locspec [lindex $argv 1]] == ""
    || [set locuin [lindex $argv 2]] == ""} \
{
	puts stderr "Usage: logconv.tcl <servtype> <local-spec> <local-uin>"
	exit 1
}

proc quote {nick} {
	if {[string first " " $nick] != -1} {
		return \"[string map {"\"" "\\\""} $nick]\"
	}
	return $nick
}

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

proc monthconv {month} {
	switch -- $month {
		Jan {return 01} Feb {return 02} Mar {return 03} Apr {return 04}
		May {return 05} Jun {return 06} Jul {return 07} Aug {return 08}
		Sep {return 09} Oct {return 10} Nov {return 11} Dec {return 12}
	}
	puts stderr "unknown month."
	exit 1
}

# expands contact to nick/uin.
proc contexp {nick} {
	global contact_uin
	if {[regexp {^[0-9]+$} $nick]} {
		return [list $nick ""]
	} elseif {![info exists contact_uin($nick)]} {
		puts stderr "Warning: $nick not found in contact list."
		return [list 0 $nick]
	} else {
		return [list $contact_uin($nick) $nick]
	}
}

# converts types
proc typeconv {args} {
	set result {}
	foreach ty $args {
		if {[string index $ty 1] == "t"} {
			lappend result tcp[string range $ty 2 end]
		} else {
			lappend result icq[string range $ty 2 end]
		}
	}
	return $result
}

foreach file {~/.micq/micqrc ~/.micq/alter} {
	if {![file exists $file]} \
		continue
	set fd [open $file r]
	set state SKIP
	while {[set line [gets $fd]] != "" || ![eof $fd]} {
		if {$state == "SKIP"} {
			if {$line == "\[Contacts\]"} {
				set state CONT
			}
			continue
		}
		if {[regexp {^[^0-9]*([0-9]+) (.+)$} $line match uin nick]} {
			set contact_nick($uin) $nick
			set contact_uin($nick) $uin
			#puts stderr "$nick ==> $uin"
		}
	}
	close $fd
}

set lino 0; set suff ""; set state OUTS
fconfigure stdin -translation lf

while {[set line [gets stdin]] != "" || ![eof stdin]} {
	incr lino
	switch -- $state {
		OUTS {
			if {$line == "<"} {
				set state LINE
				continue
        	} elseif {[regexp {^# \+([0-9]*)/([0-9]+)(/-?[0-9]+)? ([^\[]*)\[([^:]+):([^\]]+)\]([^\[]+)\[([^:]+):([^\]]+)\](.*)$} $line match nlin stamp diff pr1 ty1 su1 pr2 ty2 su2 rest]} {
				lset [typeconv $ty1 $ty2] ty1 ty2
				set intro "# $stamp$diff $pr1\[$ty1:$su1\]$pr2\[$ty2:$su2\]"
				set suff $rest
				if {$nlin != "" && $nlin != 0} {
					set left $nlin; set state INNE
				} else {
					puts $intro$suff
				}
			} elseif {[regexp {^# \+([0-9]*)/([0-9A-F]+)(/-?[0-9]+)? ([^\[]*)\[([^:]+):([^\]]+)\]([^\[]+)\[([^:]+):([^\]]+)\](.*)$} $line match nlin stamp diff pr1 ty1 su1 pr2 ty2 su2 rest]} {
				if {$diff == "/0"} {
					set diff ""
				}
				lset [typeconv $ty1 $ty2] ty1 ty2
				set intro "# [clock format [scan $stamp %x] -format %Y%m%d%H%M%S -gmt 1]$diff $pr1\[$ty1:$su1\]$pr2\[$ty2:$su2\]"
				set suff $rest
				if {$nlin != "" && $nlin != 0} {
					set left $nlin; set state INNE
				} else {
					puts $intro$suff
				}
			} elseif {[regexp {^# ([0-9]+)(/-?[0-9]+)? ([^ ]+ [^ ]+ [^ ]+)( \+[0-9]+)?(.*)$} $line match stamp diff mid nlin rest]} {
				set intro "# $stamp$diff $mid"
				set suff $rest
				set nlin [string trim $nlin " +"]
				if {$nlin != "" && $nlin != 0} {
					set left $nlin; set state INNE
				} else {
					puts $intro$suff
				}
			} else {
				puts stderr "$lino: $line"
			}
		}
		LINE {
			if {[regexp {^[A-Z][a-z][a-z] ([A-Z][a-z][a-z]) ([0-9][0-9]| [0-9]) ([0-9][0-9]):([0-9][0-9]):([0-9][0-9]) ([0-9][0-9][0-9][0-9]) (.*)$} $line match month day hour min sec year rest]} {
				#month day hour min sec year rest
				set month [monthconv $month]
				if {[string index $day 0] == " "} {set day 0[string range $day 1 end]}
				set leap [expr 30 - [string trimleft [clock format [clock scan $year$month${day}T${hour}0030] -format %S] 0]]
				set stamp [clock format [expr $leap + [clock scan $year$month${day}T$hour$min$sec]] -format %Y%m%d%H%M%S -gmt 1]

				if {[regexp {^You sent instant message to ([^()]*)$} $rest match nick]} {
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$servtype:$locuin\]!$locspec -> [quote $nick]\[$servtype:$uin\]"
				} elseif {[regexp {^You received (instant|tcp|TCP|URL) message( type [0-9]+)?( \(TCP\? [01]\))? from ([^()]+)$} $rest match src type tcp nick]} {
					set conntype $servtype
					if {$src == "URL"} {
						set type 4
					} elseif {$src == "TCP" || $src == "tcp" || $tcp == " (TCP? 1)"} {
						set conntype "tcp[string range $servtype 3 end]"
					}
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$conntype:$locuin\]!$locspec <- [quote $nick]\[$conntype:$uin\]"
					if {$type != "" && $type != " type 1"} {
						set suff " ([string range $type 6 end])"
					}
				} elseif {[regexp {^User logged on ([^()]*)$} $rest match nick]} {
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$servtype:$locuin\]!$locspec :+ [quote $nick]\[$servtype:$uin\]"
				} elseif {[regexp {^User logged on ([^()]*) \(([0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f][0-9a-f])\)$} $rest match nick status]} {
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$servtype:$locuin\]!$locspec :+ [quote $nick]\[$servtype:$uin+$status\]"
				} elseif {[regexp {^User logged on ([^()]*) \(([0-9]+)\)$} $rest match nick status]} {
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$servtype:$locuin\]!$locspec :+ [quote $nick]\[$servtype:$uin+[format %X $status]\]"
				} elseif {[regexp {^User logged off ([^()]*)$} $rest match nick]} {
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$servtype:$locuin\]!$locspec :- [quote $nick]\[$servtype:$uin+FFFFFFFF\]"
				} elseif {[regexp {^Received ACK for #([0-9]+) to ([^()]*)$} $rest match mid nick]} {
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$servtype:$locuin\]!$locspec <# [quote $nick]\[$servtype:$uin+FFFFFFFF\]"
					lappend message $mid
				} elseif {[regexp {^Added to the contact list by ([^()]*)$} $rest match nick]} {
					lset [contexp $nick] uin nick
					set intro "# $stamp \[$servtype:$locuin\]!$locspec <* [quote $nick]\[$servtype:$uin\]"
				} else {
					puts stderr ">>> $rest"
				}
				# "$month $day $hour $min $sec $year"
			}
			set state INNE
		}
		INNE {
			if {[info exists left] && [incr left -1] == 0} {
				lappend message $line
				puts "$intro +[llength $message]$suff"
				puts [join $message "\n"]
				set state OUTS; unset left message; set suff ""
				continue
			} elseif {$line == ">"} {
				if {[info exists message]} {
					puts "$intro +[llength $message]$suff"
					puts [join $message "\n"]
					unset message
				} else {
					puts "$intro$suff"
				}
				set state OUTS; set suff ""
				continue
			}
			lappend message $line
		}
	}
}

