#!/usr/bin/perl -i
# Conditional gcode post-processor
# Written by Joseph Lenox, modified by Florens Wasserfall
# Executes statements beginning with ;_if

# If it's true, then print contents of the line after the second ; until end of line.
# Else print nothing.
use strict;
use warnings;

while (<>) {
  if ($_ =~ m/^;[ ]*_if/) {
    my @tokens = split(/;/, $_);
    # modify $_ here before printing
    my $result = 0;
    # search for an expression in the form "_if 3 == 3" or "_if 2 < 5.23"
    if ($tokens[1] =~ /^_if(\s*\d+\s*)(==|<|>)(\s*\d+\s*)/) {
      my $a = $1;
      my $b = $3;
      my $operator = $2;
      if($operator eq "==") {$result = $a == $b;}
      if($operator eq "<")  {$result = $a < $b;}
      if($operator eq ">")  {$result = $a > $b;}
    }
    if ($result) {
      my $original_line = $_;
      $_ = join('', @tokens[2..$#tokens]);
      # remove leading whitespaces and trailing newline
      $_ =~ s/^\s+|\n+$//g;
      # append original conditional statement as comment for verbosity
      $_ .= "   ; line modified by conditional: " . $original_line;
     } else {
      $_ = "";
     }
  }
  print;
}