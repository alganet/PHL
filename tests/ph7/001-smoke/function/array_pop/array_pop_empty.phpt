--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_pop on an empty array returns NULL
--FILE--
<?php
$empty = array();
$val = array_pop($empty);
echo (is_null($val) ? 'NULL' : $val) . PHP_EOL;
?>
--EXPECT--
NULL
--CLEAN--
<?php
unset($empty, $val);
