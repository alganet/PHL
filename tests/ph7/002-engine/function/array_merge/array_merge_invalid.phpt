--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: array_merge with non-array arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = array_merge(1, 2);
var_dump($result);
?>
--EXPECT--
array(2) {
 [0] =>
  int(1)
 [1] =>
  int(2)
 }
