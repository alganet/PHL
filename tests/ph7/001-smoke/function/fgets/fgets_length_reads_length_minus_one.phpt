--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
fgets with a length reads at most length-1 bytes (one byte reserved for the terminator)
--FILE--
<?php
$fn = tempnam(sys_get_temp_dir(), 'ph7_fgets');
file_put_contents($fn, 'abcdefghij');   // 10 bytes, no newline
$f = fopen($fn, 'r');
var_dump(fgets($f, 5));   // reads 4 bytes
var_dump(fgets($f, 1));   // reads 0 bytes -> false
fclose($f);
// Chunked read of a long unbroken line: each fgets(4) yields 3 bytes.
$f = fopen($fn, 'r');
$out = '';
while (($chunk = fgets($f, 4)) !== false) {
    $out .= $chunk . '|';
}
echo $out, "\n";
fclose($f);
unlink($fn);
?>
--EXPECT--
string(4) "abcd"
bool(false)
abc|def|ghi|j|
--CLEAN--
<?php
