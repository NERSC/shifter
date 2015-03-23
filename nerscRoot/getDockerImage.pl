#!/usr/bin/perl

use IO::Socket::INET;

my $dockergw_host = "128.55.50.83";
my $dockergw_port = "7777";
my $ret = 0;

if (scalar(@ARGV) != 1) {
    usage(1);
}
my $image = $ARGV[0];
if ($image !~ /:/) {
    usage(1);
}
my $system = undef;
if (-e "/etc/clustername") {
    $system = `cat /etc/clustername`;
}
if (!defined($system)) {
    print STDERR "Unknown system.\n";
    usage(1);
}

my $socket = new IO::Socket::INET (
    PeerHost => $dockergw_host,
    PeerPort => $dockergw_port,
    Proto => 'tcp',
) or die("Failed to connect to dockergw");
$socket->send("$image $nerscHost\n");
my $result = "";
while (<$socket>) {
    my $data = $_;
    $data =~ s/^s\+//g;
    $data =~ s/\s+$//g;
    if ($data =~ /^ID:\s+(\S+)$/) {
        $result = $1;
    }
    if ($data =~ /^ERR:\s+(.*)$/) {
        $result = $1;
        $ret = 1;
    }
}
$socket->close();
print "$result\n";
exit $ret;


sub usage {
    my $ret = shift;
    print "getDockerImage.pl takes exactly one argument - the docker image\n";
    exit $ret;
}
