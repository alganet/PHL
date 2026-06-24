--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
yield from: delegate over an array (values + keys)
--SKIPIF--
<?php if (function_exists('zend_version') && version_compare(PHP_VERSION, '7.0.0', '<')) echo 'skip Requires PHP 7.0+'; ?>
--FILE--
<?php
function g() { yield from [1, 2, 3]; }
echo implode(",", iterator_to_array(g(), false)), "\n";
function h() { yield from ["a" => 10, "b" => 20]; }
foreach (h() as $k => $v) { echo "$k=$v\n"; }
?>
--EXPECT--
1,2,3
a=10
b=20
--CLEAN--
<?php
