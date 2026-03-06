--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_keys returns all keys of an associative array
--FILE--
<?php
$a = array('x' => 1, 'y' => 2, 'z' => 3);
$k = array_keys($a);
echo implode(',', $k);
?>
--EXPECT--
x,y,z
--CLEAN--
<?php
unset($a, $k);
