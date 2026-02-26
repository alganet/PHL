--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pop modifies the original array by removing its last element
--FILE--
<?php
$a = array('x','y','z');
array_pop($a);
echo implode(',', $a) . PHP_EOL;
?>
--EXPECT--
x,y
