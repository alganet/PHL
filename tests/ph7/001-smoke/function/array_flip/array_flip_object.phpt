--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_flip with object
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class TestClass {
    public $prop = "value";
}

 // Test array_flip with object
 $obj = new TestClass();
 $result = array_flip($obj);
 echo "Object flip: " . ($result === null ? "PASS" : "FAIL") . "\n";
?>
--EXPECT--
Object flip: PASS
--CLEAN--
<?php
unset($obj, $result);
