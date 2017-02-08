#!/usr/bin/perl

use warnings;
use strict;

use Test::More;

my $C2FFI = $ENV{'C2FFI'} || 'c2ffi';

plan tests => 4;

my $header = 't/headers/casing.h';
my $res = system("$C2FFI $header > casing-output");
if ($res != 0) {
    die "$C2FFI against $header failed.";
}

my @lines =
    map { chomp; $_ }
        `cat casing-output | ./dale-autowrap --casing=none`;
is_deeply(\@lines,
          [split /\n/, <<EOF],
(def first_test (fn extern-c int ((a int) (b float))))
(def secondTest (fn extern-c int (void)))
(def ThirdTest (fn extern-c int (void)))
EOF
          "Result unchanged where casing not applied");

@lines =
    map { chomp; $_ }
        `cat casing-output | ./dale-autowrap --casing=standard`;
is_deeply(\@lines,
          [split /\n/, <<EOF],
(def first_test (fn extern-c int ((a int) (b float))))
(def second_test (fn extern-c int (void) (secondTest)))
(def third_test (fn extern-c int (void) (ThirdTest)))
EOF
          "Standard case applied successfully");

@lines =
    map { chomp; $_ }
        `cat casing-output | ./dale-autowrap --casing=camel`;
is_deeply(\@lines,
          [split /\n/, <<EOF],
(def FirstTest (fn extern-c int ((a int) (b float)) (first_test a b)))
(def SecondTest (fn extern-c int (void) (secondTest)))
(def ThirdTest (fn extern-c int (void)))
EOF
          "Camel case applied successfully");

@lines =
    map { chomp; $_ }
        `cat casing-output | ./dale-autowrap --casing=lisp`;
is_deeply(\@lines,
          [split /\n/, <<EOF],
(def first-test (fn extern-c int ((a int) (b float)) (first_test a b)))
(def second-test (fn extern-c int (void) (secondTest)))
(def third-test (fn extern-c int (void) (ThirdTest)))
EOF
          "Lisp case applied successfully");

unlink("casing-output");

1;
