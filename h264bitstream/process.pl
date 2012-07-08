undef $/;
$code = <>;
$/ = "\n";

$decl = '';
while ($code =~ m{\n(void structure.*)}mg)
{
    $decl .= $1 . ";\n";
}

$code =~ s{#function_declarations}{$decl};

$code_read = $code;
$code_read =~ s{^(\s*) value \s* \( \s* ([^,]*) , (.*) \);}{ &proc_value_read($2, $3, $1) }exmg;
$code_read =~ s{structure\( (\w+) \)}{read_$1}xg;
print $code_read;

$code_write = $code;
$code_write =~ s{^(\s*) value \s* \( \s* ([^,]*) , (.*) \);}{ &proc_value_write($2, $3, $1) }exmg;
$code_write =~ s{structure\( (\w+) \)}{write_$1}xg;
print $code_write;

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
