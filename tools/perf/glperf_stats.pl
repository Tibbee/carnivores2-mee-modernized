#!/usr/bin/perl
# Strict numeric GLPerf CSV summary. No game launch or file writes.
use strict;
use warnings;
use POSIX qw(isfinite);

sub usage {
    print "Usage: perl tools/perf/glperf_stats.pl capture.csv [capture.csv ...]\n";
    print "Median averages the middle pair; p95/p99 use nearest rank.\n";
    print "GPU and previous-frame -1 sentinels are excluded, not timings.\n";
}
if (@ARGV == 1 && $ARGV[0] eq '--help') { usage(); exit 0; }
if (!@ARGV) { usage(); exit 2; }

sub stats {
    my ($values) = @_;
    return 'n=0 (unavailable)' unless @$values;
    my @s = sort { $a <=> $b } @$values;
    my $n = @s;
    my $sum = 0; $sum += $_ for @s;
    my $median = ($s[int(($n-1)/2)] + $s[int($n/2)]) / 2;
    return sprintf('n=%d mean=%.3f med=%.3f p95=%.3f p99=%.3f',
        $n, $sum/$n, $median, $s[POSIX::ceil(.95*$n)-1], $s[POSIX::ceil(.99*$n)-1]);
}

sub summarize {
    my ($file) = @_;
    open my $fh, '<', $file or die "$file: $!\n";
    my $header = <$fh>;
    defined $header or die "$file: empty CSV\n";
    $header =~ s/\r?\n\z//;
    $header =~ s/^\xEF\xBB\xBF//;
    my @columns = split /,/, $header, -1;
    my %seen;
    for (@columns) {
        /^[A-Za-z_][A-Za-z_0-9]*$/ && !$seen{$_}++
            or die "$file: invalid or duplicate column '$_'\n";
    }
    for (qw(cpu_ms gpu_ms triangles draw_calls)) {
        $seen{$_} or die "$file: missing column '$_'\n";
    }
    my %data = map { $_ => [] } @columns;
    my $line_number = 1;
    while (my $line = <$fh>) {
        ++$line_number;
        $line =~ s/\r?\n\z//;
        my @fields = split /,/, $line, -1;
        @fields == @columns or die "$file:$line_number: wrong field count\n";
        for my $i (0..$#columns) {
            my $text = $fields[$i];
            $text =~ /\A[+-]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?\z/
                or die "$file:$line_number: nonnumeric $columns[$i]\n";
            my $value = 0 + $text;
            isfinite($value) or die "$file:$line_number: nonfinite $columns[$i]\n";
            my $sentinel = $columns[$i] eq 'gpu_ms' || $columns[$i] =~ /_gpu_ms\z/ ||
                $columns[$i] eq 'previous_frame_interval_ms' || $columns[$i] eq 'previous_swap_ms';
            next if $sentinel && $value == -1;
            $value >= 0 or die "$file:$line_number: negative $columns[$i]\n";
            push @{$data{$columns[$i]}}, $value;
        }
    }
    $line_number > 1 or die "$file: no frame rows\n";
    close $fh or die "$file: close failed: $!\n";
    print "=== $file ===\n";
    # Preserve original headline order, then include throughput and every
    # scope/workload column emitted by the current engine, in header order.
    my @order = (qw(cpu_ms gpu_ms triangles draw_calls),
        grep { $_ !~ /\A(?:frame|cpu_ms|gpu_ms|triangles|draw_calls)\z/ } @columns);
    for my $column (@order) {
        printf "%-34s %s\n", $column, stats($data{$column});
    }
    print "\n";
}
my $failed = 0;
for my $file (@ARGV) {
    eval { summarize($file); 1 } or do { warn $@; $failed = 1; };
}
exit $failed;
