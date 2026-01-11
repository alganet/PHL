--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
List construct with array unpacking
--FILE--
<?php
$array = array(1, 2, 3);
list($a, $b, $c) = $array;
echo $a . ' ' . $b . ' ' . $c . "\n";
?>
--EXPECT--
1 2 3