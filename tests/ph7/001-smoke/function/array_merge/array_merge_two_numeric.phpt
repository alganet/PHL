--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge merges two numeric arrays
--FILE--
<?php
$a = array(1, 2);
$b = array(3, 4);
$c = array_merge($a, $b);
echo $c[0] . ',' . $c[1] . ',' . $c[2] . ',' . $c[3];
?>
--EXPECT--
1,2,3,4
--CLEAN--
<?php
unset($a, $b, $c);
