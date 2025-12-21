--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test compilation error with many errors to cover uncovered lines in compile.c
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Many syntax errors to trigger error count limit and out-of-memory paths
$a = ;
$b = ;
$c = ;
$d = ;
$e = ;
$f = ;
$g = ;
$h = ;
$i = ;
$j = ;
$k = ;
$l = ;
$m = ;
$n = ;
$o = ;
$p = ;
$q = ;
$r = ;
$s = ;
$t = ;
?>
--EXPECTF--
%s 3 Error: '=': Missing/Invalid operand
%s 4 Error: '=': Missing/Invalid operand
%s 5 Error: '=': Missing/Invalid operand
%s 6 Error: '=': Missing/Invalid operand
%s 7 Error: '=': Missing/Invalid operand
%s 8 Error: '=': Missing/Invalid operand
%s 9 Error: '=': Missing/Invalid operand
%s 10 Error: '=': Missing/Invalid operand
%s 11 Error: '=': Missing/Invalid operand
%s 12 Error: '=': Missing/Invalid operand
%s 13 Error: '=': Missing/Invalid operand
%s 14 Error: '=': Missing/Invalid operand
%s 15 Error: '=': Missing/Invalid operand
%s 16 Error: '=': Missing/Invalid operand
%s 17 Error: '=': Missing/Invalid operand
%s 18 Error count limit reached,PH7 is aborting compilation
Compile error
