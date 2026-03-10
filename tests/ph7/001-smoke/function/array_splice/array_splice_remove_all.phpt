--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_splice with offset 0 and no length removes all elements
--FILE--
<?php
$a = array(1, 2, 3);
$r = array_splice($a, 0);
echo implode(',', $r) . "\n";
echo count($a);
?>
--EXPECT--
1,2,3
0
--CLEAN--
<?php
unset($a, $r);
