--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
class_exists checks if class exists
--SKIPIF--
<?php
if (!function_exists('class_exists')) { echo 'skip: class_exists not available'; }
?>
--FILE--
<?php
class TestClass {}
$result = class_exists('TestClass');
if ($result) { echo "class_exists_ok\n"; } else { echo "class_exists_failed\n"; }
$result = class_exists('NonExistentClass');
if ($result) { echo "nonexistent_exists\n"; } else { echo "nonexistent_missing\n"; }
?>
--EXPECT--
class_exists_ok
nonexistent_missing
--CLEAN--
<?php
unset($result);
