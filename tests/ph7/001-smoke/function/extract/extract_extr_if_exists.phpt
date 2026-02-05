--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
extract with EXTR_IF_EXISTS
--FILE--
<?php

$array = array('a' => 1, 'b' => 2, 'c' => 3);
$a = 10;
$d = 20;

// Test EXTR_IF_EXISTS: only extract if variable exists
extract($array, EXTR_IF_EXISTS);

echo $a . "\n"; // should be 1 (overwritten because $a exists)
echo $d . "\n"; // should be 20 (not overwritten, doesn't exist in array)

?>
--EXPECT--
1
20
--CLEAN--
<?php
unset($array, $a, $d);
