--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: stream_copy_to_stream() — the $length/$offset window and what it answers
--FILE--
<?php
$scts_path = sys_get_temp_dir() . '/phl_scts_' . getmypid() . '.txt';
file_put_contents($scts_path, str_repeat("abcde\n", 4));

$scts_pair = function () use ($scts_path) {
    return [fopen($scts_path, 'r'), fopen('php://memory', 'w+')];
};

/* A NULL or NEGATIVE $length is "the rest"; 0 copies nothing; a POSITIVE
 * $offset seeks the source first and an offset past the end copies nothing. */
foreach ([[null, 0], [7, 0], [null, 5], [3, 5], [0, 0], [-1, 0], [null, 1000], [100, 0]] as [$len, $off]) {
    [$s, $d] = $scts_pair();
    $n = stream_copy_to_stream($s, $d, $len, $off);
    rewind($d);
    printf("len=%-5s off=%-4d => %-3d %s\n", var_export($len, true), $off, $n,
           var_export(stream_get_contents($d), true));
    fclose($s);
    fclose($d);
}

/* It APPENDS at the destination's own position. */
[$s, $d] = $scts_pair();
fwrite($d, 'PRE');
$n = stream_copy_to_stream($s, $d);
rewind($d);
echo 'append: ', $n, ' ', var_export(stream_get_contents($d), true), "\n";
fclose($s);
fclose($d);

/* A source with nothing left copies nothing, and php answers 0 rather than
 * false for it -- except from a plain FILE on Windows, whose memory-mapped copy
 * reads the empty view at the end of the file as a failure (unless the size is
 * a multiple of the 64 KiB allocation granule). */
$s = fopen('php://memory', 'r+');
$d = fopen('php://memory', 'w+');
var_dump(stream_copy_to_stream($s, $d));
fclose($s);
fclose($d);
[$s, $d] = $scts_pair();
stream_copy_to_stream($s, $d);
var_dump(stream_copy_to_stream($s, $d) === (PHP_OS_FAMILY === 'Windows' ? false : 0));
fclose($s);
fclose($d);

/* It starts where the SCRIPT is, not where a buffered read left the device. */
$s = fopen($scts_path, 'r');
fgets($s);
$d = fopen('php://memory', 'w+');
echo 'after fgets: ', stream_copy_to_stream($s, $d), "\n";
fclose($s);
fclose($d);

/* And both handles are screened by name. (Each is held and closed: Windows
 * refuses to unlink a file that still has one open.) */
$scts_a = fopen($scts_path, 'r');
try { stream_copy_to_stream('x', $scts_a); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
fclose($scts_a);
$scts_b = fopen($scts_path, 'r');
try { stream_copy_to_stream($scts_b, 42); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
fclose($scts_b);

/* stream_get_transports names what this build can actually open. */
echo 'tcp available: ', var_export(in_array('tcp', stream_get_transports(), true), true), "\n";

unlink($scts_path);
?>
--EXPECT--
len=NULL  off=0    => 24  'abcde
abcde
abcde
abcde
'
len=7     off=0    => 7   'abcde
a'
len=NULL  off=5    => 19  '
abcde
abcde
abcde
'
len=3     off=5    => 3   '
ab'
len=0     off=0    => 0   ''
len=-1    off=0    => 24  'abcde
abcde
abcde
abcde
'
len=NULL  off=1000 => 0   ''
len=100   off=0    => 24  'abcde
abcde
abcde
abcde
'
append: 24 'PREabcde
abcde
abcde
abcde
'
int(0)
bool(true)
after fgets: 18
TypeError: stream_copy_to_stream(): Argument #1 ($from) must be of type resource, string given
TypeError: stream_copy_to_stream(): Argument #2 ($to) must be of type resource, int given
tcp available: true
