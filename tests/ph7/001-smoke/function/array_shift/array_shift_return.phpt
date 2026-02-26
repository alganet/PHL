--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_shift returns first element of a non-empty array
--FILE--
<?php
$a = array('first','second','third');
echo array_shift($a) . PHP_EOL;
?>
--EXPECT--
first
--CLEAN--
<?php
unset($a);
