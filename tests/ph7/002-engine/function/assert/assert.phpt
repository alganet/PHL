--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert with boolean value
--SKIPIF--
<?php
if (!function_exists('assert')) { echo 'skip: assert not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP assert() behavior differs (assertions disabled by default)'; }
?>
--FILE--
<?php
$result = assert(true);
if ($result) { echo "assert_true_ok\n"; } else { echo "assert_true_failed\n"; }
$result = assert(false);
if ($result) { echo "assert_false_ok\n"; } else { echo "assert_false_failed\n"; }
?>
--EXPECTF--
assert_true_ok
%s Warning: assert(): Assertion failed
assert_false_failed

