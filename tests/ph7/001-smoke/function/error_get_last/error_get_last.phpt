--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
error_get_last returns backtrace array in PH7
--SKIPIF--
<?php
if (!function_exists('error_get_last')) { echo 'skip: error_get_last not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP error_get_last output differs'; }
?>
--FILE--
<?php
function foo() {
    $e = error_get_last();
    if (is_array($e)) { echo "is_array\n"; }
    if (isset($e['function'])) { echo "fn_{$e['function']}\n"; }
    if (isset($e['line'])) { echo "line_{$e['line']}\n"; }
}
foo();
?>
--EXPECT--
is_array
fn_foo
line_1
--CLEAN--
<?php
unset($e);
