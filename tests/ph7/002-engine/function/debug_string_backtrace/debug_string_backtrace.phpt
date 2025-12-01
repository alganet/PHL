--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_string_backtrace returns a string describing the backtrace
--SKIPIF--
<?php
if (!function_exists('debug_string_backtrace')) {
    echo "skip";
}
?>
--FILE--
<?php
function foo(){
    return debug_string_backtrace();
}
$s = foo();
// It must contain the symbol 'foo' from the stack trace
if (strpos($s, 'foo') !== false) echo "OK\n"; else echo "FAIL\n";
?>
--EXPECT--
OK

--CLEAN--
<?php
unset($s);
?>
