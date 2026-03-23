--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with already reverse-sorted array preserves order
--FILE--
<?php
$a = array('a' => 3, 'b' => 2, 'c' => 1);
arsort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
a: 3
b: 2
c: 1
--CLEAN--
<?php
unset($a);
