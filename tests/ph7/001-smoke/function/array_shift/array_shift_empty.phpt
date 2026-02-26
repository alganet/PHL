--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_shift on an empty array returns NULL
--FILE--
<?php
$b = array();
$val = array_shift($b);
echo (is_null($val) ? 'NULL' : $val) . PHP_EOL;
?>
--EXPECT--
NULL
--CLEAN--
<?php
unset($b, $val);
