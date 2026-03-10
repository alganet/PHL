--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with empty replacement array just removes
--FILE--
<?php
$a = array(1, 2, 3, 4, 5);
$r = array_splice($a, 1, 2, array());
echo implode(',', $r) . "\n";
echo implode(',', $a);
?>
--EXPECT--
2,3
1,4,5
--CLEAN--
<?php
unset($a, $r);
