--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
string to bool: only "" and "0" are false (no "00"/"false"/"on" special cases)
--FILE--
<?php
// php's exact falsy-string set
var_export((bool)""); echo "\n";
var_export((bool)"0"); echo "\n";
// everything else is true, including php-8 look-alikes ph7 used to special-case
var_export((bool)"00"); echo "\n";
var_export((bool)"000"); echo "\n";
var_export((bool)"0.0"); echo "\n";
var_export((bool)" "); echo "\n";
var_export((bool)"false"); echo "\n";
var_export((bool)"FALSE"); echo "\n";
var_export((bool)"on"); echo "\n";
var_export((bool)"off"); echo "\n";
var_export((bool)"yes"); echo "\n";
var_export((bool)"no"); echo "\n";
var_export((bool)"true"); echo "\n";
var_export((bool)"\0"); echo "\n";
// the same set drives conditions and loose bool comparison
echo ("00" ? "t" : "f"), ("false" ? "t" : "f"), ("0" ? "t" : "f"), ("" ? "t" : "f"), "\n";
var_export("false" == true); echo "\n";
var_export("00" == false); echo "\n";
var_export(!"false"); echo "\n";
?>
--EXPECT--
false
false
true
true
true
true
true
true
true
true
true
true
true
true
ttff
true
false
false
--CLEAN--
<?php
