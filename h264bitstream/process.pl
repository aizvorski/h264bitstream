while ($line = <>)
{
    $line =~ s{(\s*) value \s* \( \s* ([^,]*) , (.*) \);}{ &proc_value($2, $3, $1) }ex;
    print $line;
}

sub proc_value
{
    my ($s, $values, $indent) = @_;
    $values =~ s{^\s*}{};
    $values =~ s{\s*$}{};

    my $code;
    if ($values =~ m{u\((\d+)\)}) { $code = "$s = bs_read_u(b, $1);"; }
    elsif ($values =~ m{u8}) { $code = "$s = bs_read_u8(b);"; }
    elsif ($values =~ m{(ue|se|ce)}) { $code = "$s = bs_read_$1(b);"; }

    if ($values eq 'ae') { $code = "$s = bs_read_ae(b);"; }
    elsif ($values =~ m{ae}) { $code = "if (cabac) { $s = bs_read_ae(b); } else { $code }"; }

    return $code;
}
