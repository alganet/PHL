--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_key_exists: float key is truncated to integer
--FILE--
<?php
$a = array(1 => 'val');
echo array_key_exists(1.9, $a) ? 'true' : 'false';
echo PHP_EOL;
?>
--EXPECTF--
Error [8192]: Implicit conversion from float 1.9 to int loses precision in %s on line %d
true
--CLEAN--
<?php
unset($a);
