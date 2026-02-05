--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Object cast to array
--FILE--
<?php
class TestClass {
    public $publicVar = 'public';
    private $privateVar = 'private';
    protected $protectedVar = 'protected';
}

$obj = new TestClass();
$array = (array) $obj;
var_dump($array);
?>
--EXPECT--
array(3) {
 [publicVar] =>
  string(6 'public')
 [privateVar] =>
  string(7 'private')
 [protectedVar] =>
  string(9 'protected')
 }
--CLEAN--
<?php
unset($obj, $array);
