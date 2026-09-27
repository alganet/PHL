--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fscanf() scans ONE LINE per call, and answers false only at end of stream
--FILE--
<?php
/* fscanf() is sscanf() over the next LINE of a stream -- the newline included,
 * which is what a trailing `%s` or `%[` stops at. Its own answer is FALSE, and
 * it means the STREAM is finished: a line that converts nothing is still the
 * NULL / -1 answer sscanf() would have given, so `while (($r = fscanf(...)) !==
 * false)` is the loop that terminates and `!== null` is not. */
$path = tempnam(sys_get_temp_dir(), 'phlfscanf');
file_put_contents($path, "1 2\n3 4\nbob 5\n\nlast\n");

$h = fopen($path, 'r');
while (($r = fscanf($h, '%s %d')) !== false) {
    echo 'row -> ', str_replace("\n", '', var_export($r, true)), "\n";
}
var_dump(feof($h));
fclose($h);

echo "## the same file through variables\n";
$h = fopen($path, 'r');
echo 'count -> ', var_export(fscanf($h, '%d %d', $a, $b), true), " a=$a b=$b\n";
echo 'count -> ', var_export(fscanf($h, '%d %d', $c, $d), true), " c=$c d=$d\n";
echo 'count -> ', var_export(fscanf($h, '%d %d', $e, $f), true), "\n";
fclose($h);

echo "## a line the format cannot start on\n";
$h = fopen($path, 'r');
var_dump(fscanf($h, '%d'), fscanf($h, '%d'), fscanf($h, '%d'), fscanf($h, '%d'));
fclose($h);

echo "## a stream with no trailing newline\n";
file_put_contents($path, 'abc');
$h = fopen($path, 'r');
var_dump(fscanf($h, '%s'), fscanf($h, '%s'));
fclose($h);

echo "## the format is validated here too\n";
$h = fopen($path, 'r');
try {
    fscanf($h, '%z');
} catch (\Throwable $ex) {
    echo get_class($ex), ': ', $ex->getMessage(), "\n";
}
fclose($h);
unlink($path);
--EXPECT--
row -> array (  0 => '1',  1 => 2,)
row -> array (  0 => '3',  1 => 4,)
row -> array (  0 => 'bob',  1 => 5,)
row -> NULL
row -> array (  0 => 'last',  1 => NULL,)
bool(true)
## the same file through variables
count -> 2 a=1 b=2
count -> 2 c=3 d=4
count -> 0
## a line the format cannot start on
array(1) {
  [0]=>
  int(1)
}
array(1) {
  [0]=>
  int(3)
}
array(1) {
  [0]=>
  NULL
}
NULL
## a stream with no trailing newline
array(1) {
  [0]=>
  string(3) "abc"
}
bool(false)
## the format is validated here too
ValueError: Bad scan conversion character "z"
