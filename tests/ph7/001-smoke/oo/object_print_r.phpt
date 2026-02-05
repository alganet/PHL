--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object print_r functionality
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class TestClass {
    public $public_attr = "test";
    private $private_attr = 42;
}

$obj = new TestClass();
$output = print_r($obj, true);
echo $output;
?>
--EXPECTF--
Object(TestClass) {
 ['public_attr'] =>
  test
 ['private_attr'] =>
  42
 }
--CLEAN--
<?php
unset($obj, $output);
