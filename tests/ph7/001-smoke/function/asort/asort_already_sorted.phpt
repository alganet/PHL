--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with already sorted array preserves order
--FILE--
<?php
$a = array('a' => 1, 'b' => 2, 'c' => 3);
asort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
a: 1
b: 2
c: 3
--CLEAN--
<?php
unset($a);
