--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert with string expression
--SKIPIF--
<?php
if (!function_exists('assert')) { echo 'skip: assert not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP assert() behavior differs (assertions disabled by default)'; }
?>
--FILE--
<?php
$result = assert('1 == 1');
if ($result) { echo "string_true_ok\n"; } else { echo "string_true_failed\n"; }
$result = assert('1 == 0');
if ($result) { echo "string_false_ok\n"; } else { echo "string_false_failed\n"; }
?>
--EXPECTF--
string_true_ok
%s Warning:  assert(): Assertion failed
string_false_failed
--CLEAN--
<?php
unset($result);
