--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert_options with BAIL option
--SKIPIF--
<?php
if (!function_exists('assert_options')) { echo 'skip: assert_options not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP assert() behavior differs'; }
?>
--FILE--
<?php
assert_options(4, true); // PH7_ASSERT_BAIL
echo "before_assert\n";
assert(false);
echo "after_assert\n";
?>
--EXPECTF--
before_assert
%s Warning: assert(): Assertion failed

