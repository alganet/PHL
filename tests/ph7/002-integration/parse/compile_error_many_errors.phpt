--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test compilation error with many errors to cover uncovered lines in compile.c
--SKIPIF--
<?php
// php ABORTS at the first compile error; PHL keeps compiling and reports every one
// it finds (then "Error count limit reached" past 15). That is a deliberate engine
// difference, not a fidelity gap -- reporting the whole batch is more useful for an
// embedded engine -- so the two can never agree on this output. The FIRST error's
// text is what has to match php, and that is asserted by the single-error tests in
// this directory; this test exists to pin PHL's continuation behavior.
if (function_exists('zend_version')) { echo 'skip php aborts at the first compile error; PHL reports all (engine design)'; }
?>
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
%AParse error:%Asyntax error, unexpected token ";"%AFatal error:%AError count limit reached,PH7 is aborting compilation%A
--CLEAN--
<?php
unset($a, $b, $c, $d, $e, $f, $g, $h, $i, $j, $k, $l, $m, $n, $o, $p, $q, $r, $s, $t);
