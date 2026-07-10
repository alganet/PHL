--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
$GLOBALS element writes create real globals (PHP 8.1 semantics)
--FILE--
<?php
// element write at top level creates a global
$GLOBALS["nv"] = "val";
echo $nv, "|", $GLOBALS["nv"], "\n";
// overwrite existing
$x = 5; $GLOBALS["x"] = 6; echo $x, "\n";
// unset element unsets the global
unset($GLOBALS["x"]); echo isset($x) ? "set" : "unset", "\n";
// element write from inside a function
function globals_write_helper() { $GLOBALS["infunc"] = 9; }
globals_write_helper(); echo $infunc, "\n";
// int key -> decimal-named global
$GLOBALS[7] = "seven"; echo $GLOBALS["7"], "|", $GLOBALS[7], "\n";
// copy semantics
$arr = $GLOBALS; $arr["copy"] = 1;
echo isset($GLOBALS["copy"]) ? "leaked" : "copy-ok", "\n";
$b = []; $b[] = $GLOBALS;
echo count($b[0]) > 0 ? "copied" : "empty", "\n";
$b[0]["zz"] = 1; echo isset($GLOBALS["zz"]) ? "leaked" : "copy-ok", "\n";
// by-ref element write binds a global reference
$src = 5;
$GLOBALS["ref"] =& $src;
$src = 9;
echo $ref, "\n";
// existing key visible through the live view after creation via $GLOBALS
echo array_key_exists("nv", $GLOBALS) ? "in-view" : "missing", "\n";
// write then read through the variable and back
$GLOBALS["rt"] = 1; $rt++; echo $GLOBALS["rt"], "\n";
--EXPECT--
val|val
6
unset
9
seven|seven
copy-ok
copied
copy-ok
9
in-view
2
--CLEAN--
<?php
unset($nv, $infunc, $arr, $b, $src, $ref, $rt, $GLOBALS["7"]);
