--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_search should find NULL values in arrays (special NULL comparison)
--FILE--
<?php
$a = array('a' => null, 'b' => 1);
$k = array_search(null, $a, true); // strict search for null
echo $k . PHP_EOL;
?>
--EXPECT--
a
