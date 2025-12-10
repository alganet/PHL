--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_string_backtrace contains called function
--SKIPIF--
<?php
if (!function_exists('debug_string_backtrace')) { echo 'skip: debug_string_backtrace not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP debug_string_backtrace output differs'; }
?>
--FILE--
<?php
function foo() {
    $s = debug_string_backtrace();
    if (strpos($s, "Called function") !== false) {
        echo "contains_called\n";
    } else {
        echo "missing_called\n";
    }
}
foo();
?>
--EXPECT--
contains_called
