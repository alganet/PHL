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
%s Error:  '=': Missing/Invalid operand %s
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, $n, $o, $p, $q, $r, $s, $t);
