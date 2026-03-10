--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
count with COUNT_RECURSIVE on self-referential array detects cycle
--FILE--
<?php
$a = array(1, 2);
$a[] =& $a;
echo count($a, COUNT_RECURSIVE) . "\n";
?>
--EXPECTF--
Error [%d]: count(): Recursion detected in %s on line %d
3
--CLEAN--
<?php
unset($a);
