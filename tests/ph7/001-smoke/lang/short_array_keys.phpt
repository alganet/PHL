--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Short array syntax: key-value pairs
--FILE--
<?php
$a = ['x' => 10, 'y' => 20, 'z' => 30];
echo $a['x'], "\n";
echo $a['y'], "\n";
echo $a['z'], "\n";
?>
--EXPECT--
10
20
30
--CLEAN--
<?php
unset($a);
