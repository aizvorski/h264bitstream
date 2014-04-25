#!/usr/bin/perl

#/* 
# * h264bitstream - a library for reading and writing H.264 video
# * Copyright (C) 2012 Alex Izvorski
# * 
# * Written by Alex Izvorski <aizvorski@gmail.com>
# * 
# * This library is free software; you can redistribute it and/or
# * modify it under the terms of the GNU Lesser General Public
# * License as published by the Free Software Foundation; either
# * version 2.1 of the License, or (at your option) any later version.
# * 
# * This library is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# * Lesser General Public License for more details.
# * 
# * You should have received a copy of the GNU Lesser General Public
# * License along with this library; if not, write to the Free Software
# * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# */

undef $/;
$code = <>;
$/ = "\n";

$decl = '';
while ($code =~ m{\n(void structure.*)}mg)
{
    $decl .= $1 . ";\n";
}

$code =~ s{(.*)#end_preamble}{}s;
$preamble = $1;

print $preamble;

$code =~ s{#function_declarations}{$decl};

$code_read = $code;
$code_read =~ s{^(\s*) value \s* \( \s* ([^,]*) , (.*) \);}{ &proc_value_read($2, $3, $1) }exmg;
$code_read =~ s{structure\( (\w+) \)}{read_$1}xg;
$code_read =~ s{is_reading}{1}g;
$code_read =~ s{is_writing}{0}g;
print $code_read;

$code_write = $code;
$code_write =~ s{^(\s*) value \s* \( \s* ([^,]*) , (.*) \);}{ &proc_value_write($2, $3, $1) }exmg;
$code_write =~ s{structure\( (\w+) \)}{write_$1}xg;
$code_write =~ s{is_reading}{0}g;
$code_write =~ s{is_writing}{1}g;
print $code_write;

$code_read_debug = $code;
$code_read_debug =~ s{^(\s*) value \s* \( \s* ([^,]*) , (.*) \);}{ &proc_value_read_debug($2, $3, $1) }exmg;
$code_read_debug =~ s{structure\( (\w+) \)}{read_debug_$1}xg;
$code_read_debug =~ s{is_reading}{1}g;
$code_read_debug =~ s{is_writing}{0}g;
print $code_read_debug;

sub proc_value_read
{
    my ($s, $values, $indent) = @_;
    $values =~ s{^\s*}{};
    $values =~ s{\s*$}{};

    my $code;
    if ($values =~ m{u\((.*)\)}) { $code = "$s = bs_read_u(b, $1);"; }
    elsif ($values =~ m{f\((\d+),\s*(.*)\)}) { $code = "/* $s */ bs_skip_u(b, $1);"; }
    elsif ($values =~ m{(ue|se|ce|te|me|u8|u1)}) { $code = "$s = bs_read_$1(b);"; }
    elsif ($values eq 'ae') { $code = "$s = bs_read_ae(b);"; }
    else { $code = "// ERROR: value( $s, $values );"; }

    if ($values =~ m{ae} && $values ne 'ae')
    {
        $code = "if (cabac) { $s = bs_read_ae(b); }" . "\n${indent}" . "else { $code }";
    }

    return $indent . $code;
}

sub proc_value_read_debug
{
    my ($s, $values, $indent) = @_;
    $values =~ s{^\s*}{};
    $values =~ s{\s*$}{};

    my $code;
    if ($values =~ m{u\((.*)\)}) { $code = "$s = bs_read_u(b, $1);"; }
    elsif ($values =~ m{f\((\d+),\s*(.*)\)}) { $code = "int $s = bs_read_u(b, $1);"; }
    elsif ($values =~ m{(ue|se|ce|te|me|u8|u1)}) { $code = "$s = bs_read_$1(b);"; }
    elsif ($values eq 'ae') { $code = "$s = bs_read_ae(b);"; }
    else { $code = "// ERROR: value( $s, $values );"; }

    if ($values =~ m{ae} && $values ne 'ae')
    {
        $code = "if (cabac) { $s = bs_read_ae(b); }" . "\n${indent}" . "else { $code }";
    }

    $code = "printf(\"\%d.\%d: \", b->p - b->start, b->bits_left); ".
        $code .
        " printf(\"$s: \%d \\n\", $s); ";

    return $indent . $code;
}

sub proc_value_write
{
    my ($s, $values, $indent) = @_;
    $values =~ s{^\s*}{};
    $values =~ s{\s*$}{};

    my $code;
    if ($values =~ m{u\((.*)\)}) { $code = "bs_write_u(b, $1, $s);"; }
    elsif ($values =~ m{f\((\d+),\s*(.*)\)}) { $code = "/* $s */ bs_write_u(b, $1, $2);"; }
    elsif ($values =~ m{(ue|se|ce|te|me|u8|u1)}) { $code = "bs_write_$1(b, $s);"; }
    elsif ($values eq 'ae') { $code = "bs_write_ae(b, $s);"; }
    else { $code = "// ERROR: value( $s, $values );"; }

    if ($values =~ m{ae} && $values ne 'ae')
    {
        $code = "if (cabac) { bs_write_ae(b, $s); }" . "\n${indent}" . "else { $code }";
    }

    return $indent . $code;
}
