--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
asort with numeric values maintains key association
--FILE--
<?php
$a = array('x' => 10, 'y' => 5, 'z' => 20, 'w' => 1);
asort($a);
foreach ($a as $k => $v) echo "$k: $v\n";
?>
--EXPECT--
w: 1
y: 5
x: 10
z: 20
--CLEAN--
<?php
unset($a);
