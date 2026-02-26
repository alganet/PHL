--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_shift modifies the original array by removing the first element
--FILE--
<?php
$a = array('first','second','third');
array_shift($a);
echo implode(',', $a) . PHP_EOL;
?>
--EXPECT--
second,third
