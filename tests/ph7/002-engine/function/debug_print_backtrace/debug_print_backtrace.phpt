--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_print_backtrace prints a string describing the backtrace
--SKIPIF--
<?php
if (!function_exists('debug_print_backtrace')) {
    echo "skip";
}
?>
--FILE--
<?php
function b1(){
    ob_start();
    debug_print_backtrace();
    $o = ob_get_clean();
    if (strpos($o, 'b1') !== false) echo "OK\n"; else echo "FAIL\n";
}
b1();
?>
--EXPECT--
OK

--CLEAN--
<?php
unset($o);
?>
