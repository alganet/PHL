--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert with empty string returns false
--SKIPIF--
<?php
if (!function_exists('assert')) { echo 'skip: assert not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP assert() behavior differs'; }
?>
--FILE--
<?php
$result = assert('');
if ($result) { echo "empty_string_true\n"; } else { echo "empty_string_false\n"; }
?>
--EXPECTF--
%s Warning: assert(): Assertion failed
empty_string_false

