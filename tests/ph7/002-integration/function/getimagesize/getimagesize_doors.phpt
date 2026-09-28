--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
getimagesize() opens through the wrapper layer, and always writes $image_info
--FILE--
<?php
/* getimagesize() is the FILE door and getimagesizefromstring() the data one,
 * and everything else about them is shared. Three rules of the file door:
 * it opens through the wrapper layer (so data:// and php:// are ordinary
 * inputs and an unknown scheme is the wrapper refusal), a NUL in the name is
 * php's path rule rather than a truncation, and the out-parameter is created
 * EMPTY before anything is opened -- so a caller that names one reads back an
 * array even when the whole call fails. */
$tmp = tempnam(sys_get_temp_dir(), 'gis');
$gif = "GIF89a" . pack('v', 7) . pack('v', 9) . "\x87";
file_put_contents($tmp, $gif);

function gis_door(string $label, callable $probe): void {
    $msgs = [];
    set_error_handler(function ($no, $msg) use (&$msgs) { $msgs[] = "$no: $msg"; return true; });
    try { $r = $probe(); } catch (Throwable $e) { $r = get_class($e) . ': ' . $e->getMessage(); }
    restore_error_handler();
    printf("%-16s %s\n", $label, is_array($r) ? json_encode($r) : var_export($r, true));
    foreach ($msgs as $m) { printf("%-16s   %s\n", '', $m); }
}

echo "## the file door\n";
gis_door('file', fn() => getimagesize($tmp));
gis_door('data uri', fn() => getimagesize('data://image/gif;base64,' . base64_encode($gif)));
gis_door('php://memory', fn() => getimagesize('php://memory'));
gis_door('unknown scheme', fn() => getimagesize('zzz://nope'));

echo "## \$image_info exists even when nothing was read\n";
$info = 'untouched';
var_dump(getimagesize($tmp, $info));
var_dump($info);
$info = 'untouched';
$missing = $tmp . '.nope';
set_error_handler(fn() => true);
var_dump(getimagesize($missing, $info));
restore_error_handler();
var_dump($info);

echo "## a NUL in the name is refused, not truncated\n";
try { getimagesize($tmp . "\0.png"); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
var_dump(getimagesizefromstring("GIF89a" . pack('v', 3) . pack('v', 4) . "\x00")[0]);

echo "## the by-reference argument is one\n";
try { getimagesize($tmp, 1 + 1); }
catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }

unlink($tmp);
--EXPECT--
## the file door
file             {"0":7,"1":9,"2":1,"3":"width=\"7\" height=\"9\"","bits":8,"channels":3,"mime":"image\/gif","width_unit":"px","height_unit":"px"}
data uri         {"0":7,"1":9,"2":1,"3":"width=\"7\" height=\"9\"","bits":8,"channels":3,"mime":"image\/gif","width_unit":"px","height_unit":"px"}
php://memory     false
                   8: getimagesize(): Error reading from php://memory!
unknown scheme   false
                   2: getimagesize(): Unable to find the wrapper "zzz" - did you forget to enable it when you configured PHP?
                   2: getimagesize(zzz://nope): Failed to open stream: No such file or directory
## $image_info exists even when nothing was read
array(9) {
  [0]=>
  int(7)
  [1]=>
  int(9)
  [2]=>
  int(1)
  [3]=>
  string(20) "width="7" height="9""
  ["bits"]=>
  int(8)
  ["channels"]=>
  int(3)
  ["mime"]=>
  string(9) "image/gif"
  ["width_unit"]=>
  string(2) "px"
  ["height_unit"]=>
  string(2) "px"
}
array(0) {
}
bool(false)
array(0) {
}
## a NUL in the name is refused, not truncated
ValueError: getimagesize(): Argument #1 ($filename) must not contain any null bytes
int(3)
## the by-reference argument is one
Error: getimagesize(): Argument #2 ($image_info) could not be passed by reference
