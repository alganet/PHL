--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: numeric string with decimal start index is truncated to int
--FILE--
<?php
$a = array_fill("2.5", 2, 'n');
echo "CNT=" . count($a) . "\n";
echo "KEY=" . array_keys($a)[0] . "\n";
?>
--EXPECTF--
Error [8192]: Implicit conversion from float-string "2.5" to int loses precision in %s on line %d
CNT=2
KEY=2
--CLEAN--
<?php
unset($a);
