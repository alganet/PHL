--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ftruncate with a negative size throws ValueError; a valid size truncates
--FILE--
<?php
$ftn_fp = fopen('php://memory', 'r+');
fwrite($ftn_fp, "abcdef");
try {
    ftruncate($ftn_fp, -1);
    echo "NO_ERROR\n";
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
// the failed call left the stream untouched
rewind($ftn_fp);
var_dump(stream_get_contents($ftn_fp));
var_dump(ftruncate($ftn_fp, 2));
rewind($ftn_fp);
var_dump(stream_get_contents($ftn_fp));
fclose($ftn_fp);
?>
--EXPECT--
ftruncate(): Argument #2 ($size) must be greater than or equal to 0
string(6) "abcdef"
bool(true)
string(2) "ab"
--CLEAN--
<?php
