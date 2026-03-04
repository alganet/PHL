--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_push with only the array argument returns current count
--FILE--
<?php
$a = array('x', 'y');
echo array_push($a) . PHP_EOL;
?>
--EXPECT--
2
--CLEAN--
<?php
unset($a);
