--TEST--
++/-- preserve float type and numeric-string semantics
--DESCRIPTION--
Regression: PH7_OP_DECR operated on the wrong ph7_value (pTos vs pObj) and
collapsed real results to int. DECR now mirrors the hardened INCR. This test
covers the cases that are byte-for-byte identical on PHL and real PHP: float
++/-- (pre and post) keep float type, and numeric strings increment/decrement to
int. Probed with is_int()/is_float()/is_string() (not var_dump, which Xdebug
overloads). The non-numeric-string / null no-op cases live in
decrement_noop.phpt (PHP emits a deprecation there that PHL does not yet mirror).
--FILE--
<?php
// Closure in a variable, not a named function: the compat harness @includes
// every FILE into one PHP process, so a top-level function would collide.
$show = function($v){ echo (is_int($v)?'int':'-'),' ',(is_float($v)?'float':'-'),' ',(is_string($v)?'str':'-'),' val=',$v,"\n"; };
$a=1.0; $a++; $show($a);
$a=1.0; $a--; $show($a);
$a=1.0; $r=$a--; $show($r); $show($a);
$a=1.0; $show(--$a);
$s="5"; $s++; $show($s);
$s="5"; $s--; $show($s);
$s="5"; $r=$s--; $show($r);
$i=5; $i--; $show($i);
$i=5; $show($i--);
// Cache consistency: after --/++ on an integer-valued real, the cached int
// (x.iVal/MEMOBJ_INT) must be refreshed, else (int), ===, intdiv() read a stale
// integer.
$a=2.0; $a--; echo (int)$a,' ',($a===1.0?'eq':'ne'),' ',intdiv($a,1),"\n";
$a=2.0; $a++; echo (int)$a,' ',($a===3.0?'eq':'ne'),"\n";
?>
--EXPECT--
- float - val=2
- float - val=0
- float - val=1
- float - val=0
- float - val=0
int - - val=6
int - - val=4
- - str val=5
int - - val=4
int - - val=5
1 eq 1
3 eq
--CLEAN--
<?php
?>
