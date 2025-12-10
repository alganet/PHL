--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
gettype returns type of variable
--SKIPIF--
<?php
if (!function_exists('gettype')) { echo 'skip: gettype not available'; }
if (function_exists('zend_version')) { echo 'skip: PHP gettype behavior differs'; }
?>
--FILE--
<?php
$type = gettype("hello");
if ($type === "string") { echo "string_ok\n"; } else { echo "string_failed\n"; }
$type = gettype(42);
if ($type === "int") { echo "int_ok\n"; } else { echo "int_failed\n"; }
$type = gettype(array(1,2,3));
if ($type === "array") { echo "array_ok\n"; } else { echo "array_failed\n"; }
$type = gettype(null);
if ($type === "null") { echo "null_ok\n"; } else { echo "null_failed\n"; }
?>
--EXPECT--
string_ok
int_ok
array_ok
null_ok
