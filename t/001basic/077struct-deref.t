#!/usr/bin/env perl

use warnings;
use strict;
$ENV{"DALE_TEST_ARGS"} ||= "";
my $test_dir = $ENV{"DALE_TEST_DIR"} || ".";
$ENV{PATH} .= ":.";

use Data::Dumper;
use Test::More tests => 3;

my @res = `dalec $ENV{"DALE_TEST_ARGS"} $test_dir/t/src/struct-deref.dt -o struct-deref `;
is(@res, 0, 'No compilation errors');

@res = `./struct-deref`;
is($?, 0, 'Program executed successfully');

chomp for @res;
is_deeply(\@res, [
    '10', '20'
], 'Got expected results');

`rm struct-deref`;

1;
