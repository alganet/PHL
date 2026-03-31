--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: unset element from one of three sharing variables
--FILE--
<?php
$a = ["x" => 1, "y" => 2];
$b = $a;
$c = $a;
unset($b["x"]);
echo count($a) . "\n";
echo count($b) . "\n";
echo count($c) . "\n";
echo $a["x"] . "\n";
echo $c["x"] . "\n";
?>
--EXPECT--
2
1
2
1
1
--CLEAN--
<?php
unset($a, $b, $c);
