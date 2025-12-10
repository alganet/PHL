--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
debug_backtrace returns function name, args and file
--SKIPIF--
<?php
if (!function_exists('debug_backtrace')) { echo 'skip: debug_backtrace not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP debug_backtrace output differs'; }
?>
--FILE--
<?php
function foo($x, $y) {
    $bt = debug_backtrace();
    if (is_array($bt)) { echo "is_array\n"; }
    if (isset($bt['function'])) { echo "fn_{$bt['function']}\n"; }
    if (isset($bt['args'])) { echo "args_" . count($bt['args']) . "\n"; }
    if (isset($bt['line'])) { echo "line_{$bt['line']}\n"; }
    if (isset($bt['file'])) { echo "file_present\n"; }
}

foo(10, "abc");
?>
--EXPECT--
is_array
fn_foo
args_2
line_1
file_present

