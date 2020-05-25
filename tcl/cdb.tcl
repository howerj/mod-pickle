set dbf test.cdb
set klen 1000

set c [cdb open $dbf w]
puts "cdb $c"

for {set i 0} {< $i $klen} {incr i} {
	cdb write $c $i [random]
}

cdb write $c dup 1
cdb write $c dup 2
cdb write $c dup 3

cdb close $c

set c [cdb open $dbf r]

puts
puts "stats: [cdb stats $c]"
puts

for {set i 0} {< $i $klen} {incr i} {
	if {eq 0 [cdb exists $c $i]} {
		return "key $i does not exist!" -1
	}
}

set dups [cdb count $c dup]
if {ne 3 $dups} {
	return "duplicate count incorrect $dups" -1;
}

cdb close $c

#remove $test
unset c dbf klen

