--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pop returns the last element of a non-empty array
--FILE--
<?php
$a = array('x','y','z');
echo array_pop($a) . PHP_EOL;
?>
--EXPECT--
z
