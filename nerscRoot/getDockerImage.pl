#!/usr/bin/perl

use IO::Socket::INET;

my $dockergw_host = "128.55.50.83";
my $dockergw_port = "7777";

if (scalar(@ARGV) != 1) {
    usage(1);
}
my $image = $ARGV[0];
if ($image !~ /:/) {
    usage(1);
}
my $nerscHost = `cat /etc/clustername`;
my $socket = new IO::Socket::INET (
    PeerHost => $dockergw_host,
    PeerPort => $dockergw_port,
    Proto => 'tcp',
) or exit 1;
$socket->send("$image $nerscHost\n");
my $result = <$socket>;
$socket->close();
print $result;


sub usage {
    my $ret = shift;
    print "getDockerImage.pl takes exactly one argument - the docker image\n";
    exit $ret;
}
