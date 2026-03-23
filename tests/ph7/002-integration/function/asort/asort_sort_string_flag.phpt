--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with SORT_STRING flag
--FILE--
<?php
$a = array('a' => '10', 'b' => '9', 'c' => '100', 'd' => '2');
asort($a, SORT_STRING);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
a: 10
c: 100
d: 2
b: 9
--CLEAN--
<?php
unset($a);
