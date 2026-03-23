--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
arsort with numeric values maintains key association
--FILE--
<?php
$a = array('x' => 10, 'y' => 5, 'z' => 20, 'w' => 1);
arsort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
z: 20
x: 10
y: 5
w: 1
--CLEAN--
<?php
unset($a);
