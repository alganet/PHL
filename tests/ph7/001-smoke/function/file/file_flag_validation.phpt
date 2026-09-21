--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
file(): $flags is validated, "empty line" means zero-length, and CR is stripped with LF
--FILE--
<?php
$ffv_path = sys_get_temp_dir() . '/phl_file_flags_' . getmypid() . '.txt';
file_put_contents($ffv_path, "a\r\nb\n\n   \nc");
// FILE_IGNORE_NEW_LINES drops the CR as well as the LF, on every platform.
// FILE_SKIP_EMPTY_LINES skips only ZERO-LENGTH lines -- a line of spaces stays, and
// without IGNORE_NEW_LINES a bare "\n" line is not empty (the newline is still on it).
foreach ([0, 1, 2, 4, 6, 16, 22, 2 | 16] as $ffv_flag) {
    echo str_pad($ffv_flag, 3), json_encode(file($ffv_path, $ffv_flag)), "\n";
}
// Only ONE line ending comes off: the LF plus the CR directly before it. A doubled
// CR keeps the first, and a CR-terminated final line (no LF) keeps it entirely.
$ffv_cr = sys_get_temp_dir() . '/phl_file_cr_' . getmypid() . '.txt';
foreach (["a\r\r\nb\r\n", "a\nb\r", "a\r"] as $ffv_i => $ffv_body) {
    file_put_contents($ffv_cr, $ffv_body);
    echo 'cr', $ffv_i, ' ', json_encode(file($ffv_cr, FILE_IGNORE_NEW_LINES)), "\n";
}
unlink($ffv_cr);
// the mask is checked BEFORE the wrapper is resolved, so a bad flag beats a bad scheme
try {
    file("bogus://x", 8);
    echo "NO_THROW\n";
} catch (\ValueError $e) {
    echo 'wrapper: ', $e->getMessage(), "\n";
}
// FILE_APPEND belongs to file_put_contents; file() rejects it like any stray bit
foreach ([8, 32, -1, 99] as $ffv_bad) {
    try {
        file($ffv_path, $ffv_bad);
        echo "NO_THROW\n";
    } catch (\ValueError $e) {
        echo $ffv_bad, ': ', $e->getMessage(), "\n";
    }
}
unlink($ffv_path);
?>
--EXPECT--
0  ["a\r\n","b\n","\n","   \n","c"]
1  ["a\r\n","b\n","\n","   \n","c"]
2  ["a","b","","   ","c"]
4  ["a\r\n","b\n","\n","   \n","c"]
6  ["a","b","   ","c"]
16 ["a\r\n","b\n","\n","   \n","c"]
22 ["a","b","   ","c"]
18 ["a","b","","   ","c"]
cr0 ["a\r","b"]
cr1 ["a","b\r"]
cr2 ["a\r"]
wrapper: file(): Argument #2 ($flags) must be a valid flag value
8: file(): Argument #2 ($flags) must be a valid flag value
32: file(): Argument #2 ($flags) must be a valid flag value
-1: file(): Argument #2 ($flags) must be a valid flag value
99: file(): Argument #2 ($flags) must be a valid flag value
--CLEAN--
<?php
unset($e);
