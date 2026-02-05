--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object var_dump functionality
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
class TestClass {
    public $public_attr = "test";
    private $private_attr = 42;
}

$obj = new TestClass();
ob_start();
var_dump($obj);
$output = ob_get_clean();
echo $output;
?>
--EXPECTF--
object(TestClass) {
 ['public_attr'] =>
  string(4 'test')
 ['private_attr'] =>
  int(42)
 }
--CLEAN--
<?php
unset($obj, $output);
