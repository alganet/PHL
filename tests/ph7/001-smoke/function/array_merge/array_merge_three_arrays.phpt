--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_merge merges three arrays
--FILE--
<?php
$a = array(1);
$b = array(2);
$c = array(3);
$r = array_merge($a, $b, $c);
echo $r[0] . ',' . $r[1] . ',' . $r[2];
?>
--EXPECT--
1,2,3
--CLEAN--
<?php
unset($a, $b, $c, $r);
