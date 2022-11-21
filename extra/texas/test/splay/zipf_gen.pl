#!/usr/bin/perl -w

use Math::Random::Zipf;
use List::Util qw(shuffle);

$#ARGV==2 or die "usage: zipf_gen.pl range_max num_keys num_lookups\n";

($range_max, $num_keys, $num_lookups) = @ARGV;

$z = Math::Random::Zipf->new($range_max,1);

@arrayOfKeys = (0..$num_keys);

@arrayOfKeys = shuffle @arrayOfKeys;

#generate key set with no collisions
for ($i=0;$i<$num_keys;$i++) {
    print "insert ",$arrayOfKeys[$i],"\n";  
}

#generate lookup stream, including lookups that
#are for non-existent keys
for ($i=0;$i<$num_lookups;$i++) {
    print "lookup ",$z->rand(),"\n";
}
    
