--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Copying $GLOBALS takes a by-value snapshot of the symbol table (PHP 8.1)
--FILE--
<?php
$x = 1;
$snap = $GLOBALS;
$x = 2;
echo $snap["x"], "|", $GLOBALS["x"], "\n";
$snap["x"] = 99;
echo $x, "\n";
function globals_snap_helper() { $GLOBALS["ff"] = 3; return $GLOBALS; }
$s2 = globals_snap_helper(); echo $s2["ff"], "|", $ff, "\n";
foreach ($GLOBALS as $k => $v) { if ($k === "x") { echo "iter:$k=", is_array($v) ? "arr" : $v, "\n"; } }
--EXPECT--
1|2
2
3|3
iter:x=2
--CLEAN--
<?php
unset($x, $snap, $s2, $ff, $k, $v);
