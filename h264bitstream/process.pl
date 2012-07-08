undef $/;
$text = <>;
$/ = "\n";

$decl = '';
while ($text =~ m{\n(void structure.*)}mg)
{
    $decl .= $1 . ";\n";
}

$text =~ s{#function_declarations}{$decl};

$text =~ s{^(\s*) value \s* \( \s* ([^,]*) , (.*) \);}{ &proc_value_read($2, $3, $1) }exmg;
$text =~ s{structure\( (\w+) \)}{read_$1}xg;
print $text;

sub proc_value_read
{
    my ($s, $values, $indent) = @_;
    $values =~ s{^\s*}{};
    $values =~ s{\s*$}{};

    my $code;
    if ($values =~ m{u\((\d+)\)}) { $code = "$s = bs_read_u(b, $1);"; }
    elsif ($values =~ m{(ue|se|ce|te|me|u8)}) { $code = "$s = bs_read_$1(b);"; }
    elsif ($values eq 'ae') { $code = "$s = bs_read_ae(b);"; }
    else { $code = "// ERROR: value( $s, $values );"; }

    if ($values =~ m{ae} && $values ne 'ae')
    {
        $code = "if (cabac) { $s = bs_read_ae(b); }" . "\n${indent}" . "else { $code }";
    }

    return $indent . $code;
}
