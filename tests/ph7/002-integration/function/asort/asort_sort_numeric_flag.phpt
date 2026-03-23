--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with SORT_NUMERIC flag
--FILE--
<?php
$a = array('a' => '10', 'b' => '9', 'c' => '100', 'd' => '2');
asort($a, SORT_NUMERIC);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
d: 2
b: 9
a: 10
c: 100
--CLEAN--
<?php
unset($a);
