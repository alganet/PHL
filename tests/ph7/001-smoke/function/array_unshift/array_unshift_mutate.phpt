--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_unshift() modifies the original array
--FILE--
<?php
$a = array('b');
array_unshift($a, 'a');
echo implode(',', $a) . PHP_EOL;
?>
--EXPECT--
a,b
--CLEAN--
<?php
unset($a);
