--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
get_class returns class name of object
--SKIPIF--
<?php
if (!function_exists('get_class')) { echo 'skip: get_class not available'; }
?>
--FILE--
<?php
class TestClass {}
$obj = new TestClass();
$result = get_class($obj);
if ($result === 'TestClass') { echo "get_class_ok\n"; } else { echo "get_class_failed\n"; }
?>
--EXPECT--
get_class_ok
