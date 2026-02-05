--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: array_keys returns list of keys for associative arrays
--FILE--
<?php
$a = array('x' => 1, 'y' => 2, 'z' => 3);
$k = array_keys($a);
// print keys separated by comma
echo implode(',', $k) . "\n";
?>
--EXPECT--
x,y,z
--CLEAN--
<?php
unset($a, $k);
