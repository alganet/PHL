--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with duplicate values maintains stable order (PHP 8)
--FILE--
<?php
$a = array('a' => 2, 'b' => 1, 'c' => 2, 'd' => 3);
arsort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
d: 3
a: 2
c: 2
b: 1
--CLEAN--
<?php
unset($a);
