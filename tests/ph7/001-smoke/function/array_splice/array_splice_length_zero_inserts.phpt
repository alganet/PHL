--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with length 0 inserts replacement without removing
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_splice($a, 1, 0, array(99));
echo count($r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
0
1,99,2,3
--CLEAN--
<?php
unset($a, $r);
