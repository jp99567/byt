use strict;
use feature ':5.14';
use warnings;

package SomeMath;

sub new_mwa{
  my $class = shift;
  my $self = {
	kp_q => [],
	kp_len => 50,
	kp_val => 0,
	kp_denom => 0,
	epoch_prev=> 0 };
	
	my $f = shift;
	$$self{kp_len} = $f // $$self{kp_len}; 
	bless $self;
}

sub set{
	my $self = shift;
	my $v = shift;
	my $epoch = shift;
	my $dt = $epoch - $$self{epoch_prev};
	$$self{epoch_prev} = $epoch;
	my $val_product =  $v * $dt;
	push @{$$self{kp_q}}, [$val_product,$dt];
	$$self{kp_val} += $val_product;
	$$self{kp_denom} += $dt;
	
	while($$self{kp_denom} > $$self{kp_len} && int(@{$$self{kp_q}}) > 1 ){
		my ( $pv, $pdt) = @{shift(@{$$self{kp_q}})};
		$$self{kp_val} -= $pv;
		$$self{kp_denom} -= $pdt;
	}
	
	$$self{kp_val} / $$self{kp_denom};
}

package main;

use DBI;
use Data::Dumper;
use Getopt::Std;

my $dbh = DBI->connect('dbi:Pg:dbname='.$ENV{PGDATABASE}) or die $DBI::errstr;
sub dbqt($){ return $dbh->quote(shift) ;}

my %opts;
getopts('f:t:p:F:', \%opts) or die 'getopts';

sub arg2timestamp($){
	my $a = shift;
	if( $a =~ /^\d+:\d+:\d+/){
		return dbqt('today').'::timestamp + '.dbqt($a);
	}
	dbqt($a);
}

my $t_from = dbqt('now').'::timestamp - '.dbqt('08:00:00').'::time';
if(exists($opts{f})){
	$t_from = arg2timestamp $opts{f};
}

my $and_t_to = '';
if(exists($opts{t})){
	$and_t_to = 'AND t AT time zone '.dbqt('UTC').' <= '.arg2timestamp $opts{t};
}

my $filter = 1;
if(exists($opts{F})){
	$filter = int $opts{F};
}
die 'invalid filter length' unless $filter > 0; 

my @place = ('izbaSZ');
if(exists($opts{p})){
	@place = split /,/, $opts{p};
}

sub getdata($)
{
	my $place_id = shift;
	my $place_id_dbqt = dbqt($place_id);
	
	my $sql = <<ENDSQL;
SET timezone TO 'CET';
SELECT to_char(t at time zone 'UTC', 'MonDD-HH24:MI:SS'), val, extract(epoch from t)
FROM temperature,temp_meas_place
WHERE place_id=temp_meas_place.id AND val IS NOT NULL
AND temp_meas_place.name=$place_id_dbqt AND t >= $t_from $and_t_to
ORDER BY t
ENDSQL

	my $filename = '/tmp/gnuplot_temperature-'.$place_id.'.data';
	open(my $data, '>', $filename) or die $!;
	my $sth = $dbh->prepare($sql);
	$sth->execute;
	local $, = ' ';

	my $mwa = SomeMath->new_mwa($filter);
 
	while(my $row =  $sth->fetchrow_arrayref ){
		say $data $$row[0], $mwa->set($$row[1], $$row[2]);
	}
	close $data;
	return $filename;
}

my $gpbatch = <<ENDGNUPLOT;
set terminal qt persist
set xdata time
set timefmt "%b%d-%H:%M:%S"
set grid
ENDGNUPLOT

open(my $gnuplot, '|-', 'gnuplot5-qt') or die $!;

my @plots = ();
my $plot_style = 'lines';
$plot_style = 'steps' if $filter == 1; 
foreach my $place_id (@place){
	my $fn = getdata $place_id;
	my $plotstr = "\"$fn\" using 1:2 index 0 title \"$place_id\" with $plot_style";
	push @plots, $plotstr;
}

say $gnuplot $gpbatch;
say $gnuplot 'plot '.join(", \\\n", @plots);

