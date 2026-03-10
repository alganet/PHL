--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice inserts replacement into empty array
--FILE--
<?php
$a = array();
$r = array_splice($a, 0, 0, array(1, 2));
echo count($r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
0
1,2
--CLEAN--
<?php
unset($a, $r);
