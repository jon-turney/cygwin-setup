package Version;
require 5.000;
require Exporter;

$VERSION = sprintf("%d.%02d", q$Revision$ =~ /(\d+)\.(\d+)/);

@EXPORT_OK = qw(Normalize);
@EXPORT = ();

sub Normalize {
    my $fn = shift;
    my $transform = shift;
    my ($i, @v);
    local ($pre, $post);
    local($_) = $fn;
    $pre = $post = '';
    defined($transform) and &{$transform}($_);
    s!^(.*/)([^/]*)$!$2!o;
    $pre .= length($1) ? $1 : '';
    s/^((?:(?:[a-z][a-z]*\d)*[^a-z\d])+)//oi;
    $pre .= $1;
    $post .= $1 while s/([-\._](bz2|gz)|[-\._]bin|[-\._]tar|[-\._]tgz|[-\._]Z|[-\._]?elf|[-\._]lnx|[-\._][a-z]*doc[a-z]*|[-\._]linux|[-\._]hacker|[-\._]tool|[-\._]src)//;
    $_ = '0' unless length($_);
    my $rawver = $_;
    push(@v, split(/[-._]/));
    if (/-(\d+)$/o) {
	$v[$#v] = ":$v[$#v]";
    }
    for ($i = 0; $i < @v; $i++) {
	$v[$i] =~ s/^pl?// if $i < $#v;
	$v[$i] =~ /^(\d*)(.*)/o;
	if ($2 eq '') {
	    my $num = $1;
	    $num = 19 . $num if $num =~ /^99/;	# y2k kludge
	    $v[$i] = sprintf("%04d", $num);
	} elsif ($1 eq '') {
	    my $a = $2;
	    $a =~ /(\D+)(\d*)/;
	    splice(@v, $i++, 1, '0000', lc sprintf("%04.4s", $1));
	    splice(@v, $i + 1, 0, $2) if length($2);
	} else {
	    splice(@v, $i--, 1, $1, $2);
	}
    }

    return $fn unless length($_);

    $post =~ s/\.tgz/.tar.gz/;
    $post =~ s/\.bz2/.gz/;
    ($_ = join('', @v)) =~ s/ /0/g;
    return wantarray ? (($pre . $post), $_, $rawver) : $_;
}

sub sortkey {
    return ($a . (($maxlen - length($a)) x '0'))
			cmp
	   ($b . (($maxlen - length($b)) x '0'));
}

sub Sort {
    local *arr = shift;
    local $_;
    my $len;
    local $maxlen = 0;
    local $minlen = 65535;
    foreach (@arr) {
	my $len = length((split(/\\/o))[1]);
	$maxlen = $len if $len > $maxlen;
	$minlen = $len if $len < $minlen;
    }

    return sort sortkey @arr;
} 
1;
