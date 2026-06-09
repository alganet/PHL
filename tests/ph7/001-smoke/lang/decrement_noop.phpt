--TEST--
-- on a non-numeric string or null is a no-op (value unchanged)
--DESCRIPTION--
Regression: PH7_OP_DECR lacked the non-numeric-string guard, so "abc"-- coerced
to int(-1) instead of leaving the string unchanged. PHP has no string decrement,
so "abc"--, "5x"-- and null-- are no-ops. This guards the VALUE behavior.

PHL-only: on PHP 8.3+ each of these operations additionally emits a deprecation
("Decrement on non-numeric string has no effect and is deprecated") / warning
("Decrement on type null has no effect"). PHL does not yet emit engine
deprecation notices (tracked separately in the roadmap, §3.7; PHL's "abc"++
likewise emits no notice), so the combined output diverges from PHP only by those
notices. Skipped on real PHP rather than suppressing them, until PHL mirrors them.
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$s="abc"; $s--; echo "[$s]\n";
$s="a";   $s--; echo "[$s]\n";
$s="5x";  $s--; echo "[$s]\n";
$n=null;  $n--; echo ($n===null?'null-noop':'CHANGED'),"\n";
?>
--EXPECT--
[abc]
[a]
[5x]
null-noop
--CLEAN--
<?php
?>
