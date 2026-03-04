--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_push modifies the original array by appending elements
--FILE--
<?php
$a = array();
array_push($a, 'a', 'b');
echo implode(',', $a) . PHP_EOL;
?>
--EXPECT--
a,b
--CLEAN--
<?php
unset($a);
