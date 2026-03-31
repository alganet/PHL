--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
COW: string key assignment does not affect original
--FILE--
<?php
$a = ["x" => 1, "y" => 2];
$b = $a;
$b["x"] = 99;
echo $a["x"] . "\n";
echo $b["x"] . "\n";
?>
--EXPECT--
1
99
--CLEAN--
<?php
unset($a, $b);
