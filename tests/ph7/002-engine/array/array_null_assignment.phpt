--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
array null value assignment
--FILE--
<?php
$array = array('a' => 1, 'b' => 2);
$array['a'] = null;
var_dump($array['a'] === null);
var_dump(count($array));
?>
--EXPECT--
bool(TRUE)
int(2)