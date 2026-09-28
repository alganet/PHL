--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A unit with many syntax errors reports only the first
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
%AParse error:%Asyntax error, unexpected token ";"%A
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, $n, $o, $p, $q, $r, $s, $t);
