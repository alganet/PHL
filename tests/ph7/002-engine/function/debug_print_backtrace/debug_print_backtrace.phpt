--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_print_backtrace prints called function
--SKIPIF--
<?php
if (!function_exists('debug_print_backtrace')) { echo 'skip: debug_print_backtrace not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP debug_print_backtrace output differs'; }
?>
--FILE--
<?php
function foo() {
    ob_start();
    debug_print_backtrace();
    $out = ob_get_clean();
    if (strpos($out, "Called function") !== false) {
        echo "printed_called\n";
    } else {
        echo "missing_called\n";
    }
}
foo();
?>
--EXPECT--
printed_called
