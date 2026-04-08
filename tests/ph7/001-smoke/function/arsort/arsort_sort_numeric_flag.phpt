--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with SORT_NUMERIC flag
--FILE--
<?php
$a = array('a' => '10', 'b' => '9', 'c' => '100', 'd' => '2');
arsort($a, SORT_NUMERIC);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
c: 100
a: 10
b: 9
d: 2
--CLEAN--
<?php
unset($a);
