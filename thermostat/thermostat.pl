#!/usr/bin/perl

use strict;
use warnings;
use feature ':5.14';

sub getCurTemp{
	open(my $fd, '<', '/tmp/cur_temp') || die $!;
	my $val = <$fd>;
	close($fd);
	return $val;
}

sub getTargetTemp{
	open(my $fd, '<', '/tmp/target_temp') || die $!;
	my $val = <$fd>;
	close($fd);
	$val = $val*1;
	if($val >= 0 && $val < 30){	
		return $val;
	}
	return 0;
}

sub heat($){
	my $val = shift;
	my $ch = $val ? '1' : '0';
	open(my $fd, '>', '/tmp/heater') || die $!;
	print $fd $ch;
	close($fd);
	say STDOUT time(),' heating ',$ch;
}

sub isheating(){
	open(my $fd, '<', '/tmp/heater') || die $!;
	my $v = <$fd>;
	close($fd);
	return int($v);
}

my $fini_req_flag = 0;
$SIG{INT} = sub{ $fini_req_flag = 1; say "stop..."; };


sub print_state($$$){
	my ( $heating, $target, $cur) = @_;
	$heating = $heating ? 'ON' : 'OFF';
	open(my $fd, '>', '/tmp/heater_stat') || die $!;
	say $fd "heating: $heating\ntarget temp: $target deg\ncurrent temp: $cur deg";
	close($fd);
}

sub print_state2($$$$){
	my ( $heating, $target, $cur, $ratio) = @_;
	$heating = $heating ? 'ON' : 'OFF';
	open(my $fd, '>', '/tmp/heater_stat') || die $!;
	say $fd "heating: $heating\ntarget temp: $target deg\ncurrent temp: $cur deg";
	say $fd "power ratio: $ratio %";
	close($fd);
}

#naive first order
sub thermostat(){
	my $hysteresis = 0.1;
	my $sample_interval = 10;
	
	while(!$fini_req_flag){
		my $cur = getCurTemp()*1;
		my $targetValue = getTargetTemp();
		my $heating = isheating();
		
		if($heating && $cur > $targetValue + $hysteresis ){
			$heating=0;
			heat $heating;
		}
		elsif(!$heating && $cur < $targetValue - $hysteresis){
			$heating=1;
			heat $heating;
		}
		
		print_state $heating, $targetValue, $cur;
		sleep $sample_interval;
	}
	heat 0;
}

my $filtlen = 10;
my @filter;

sub avgvla($){
	push @filter, shift;
	if(int(@filter) > $filtlen){
		shift @filter;
	}
	my $avg = 0;
	$avg += $_ foreach @filter;
	say $avg/int(@filter);
	$avg/int(@filter);
}

thermostat;


