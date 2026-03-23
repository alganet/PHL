--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with duplicate values maintains stable order (PHP 8)
--FILE--
<?php
$a = array('a' => 2, 'b' => 1, 'c' => 2, 'd' => 3);
asort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
b: 1
a: 2
c: 2
d: 3
--CLEAN--
<?php
unset($a);
