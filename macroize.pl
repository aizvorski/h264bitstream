undef $/;
$text = <>;
$/ = "\n";

$text =~ s{(\w.*) = bs_read_(\w+)\(b\);}{value( $1, $2 );}g;
$text =~ s{(\w.*) = bs_read_(\w+)\(b,\s*(\w.*)\);}{value( $1, $2($3) );}g;
$text =~ s{read_(\w+)}{structure($1)}g;

print $text;
