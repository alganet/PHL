--TEST--
Integer-valued reals keep float type identity (not int)
--DESCRIPTION--
Regression: PHL stored an integer-valued real (e.g. 1.0, or the result of float
arithmetic) with both MEMOBJ_REAL and a cached MEMOBJ_INT, and is_int() reported
it as an int. The REAL flag is now authoritative: a float stays a float. Probed
with is_int()/is_float() (not var_dump, whose float label Xdebug renders as
"double") so output is identical on PHL and real PHP.
--FILE--
<?php
// Closure in a variable, not a named function: the compat harness @includes
// every FILE into one PHP process, so a top-level function would collide.
$show = function($v){ echo (is_int($v)?'int':'-'),' ',(is_float($v)?'float':'-'),' val=',$v,"\n"; };
$show(1.0);
$show(1.0 + 1.0);
$show(3.0 - 1.0);
$show(2.0 * 1.0);
$show(4.0 / 2.0);
$show(2.0 ** 2);
$show(5);
$show(1.5 + 1.5);
$show(7 / 2);
?>
--EXPECT--
- float val=1
- float val=2
- float val=2
- float val=2
- float val=2
- float val=4
int - val=5
- float val=3
- float val=3.5
--CLEAN--
<?php
?>
