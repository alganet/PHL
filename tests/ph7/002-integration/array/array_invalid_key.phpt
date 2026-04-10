--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array with object key
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test array with object key
$obj = new stdClass();
$arr = array($obj => 'value');
var_dump($arr);
?>
--EXPECTF--
%s Notice:  Missing constructor argument 1($v) for class 'stdClass'
array(1) {
 [Object] =>
  string(5) "value"
 }
--CLEAN--
<?php
unset($obj, $arr);
