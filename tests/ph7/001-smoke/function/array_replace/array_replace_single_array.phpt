--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_replace with single array returns a copy
--FILE--
<?php
$a = array(10, 20, 30);
$r = array_replace($a);
echo count($r) . "\n";
echo $r[0] . ',' . $r[1] . ',' . $r[2];
?>
--EXPECT--
3
10,20,30
--CLEAN--
<?php
unset($a, $r);
