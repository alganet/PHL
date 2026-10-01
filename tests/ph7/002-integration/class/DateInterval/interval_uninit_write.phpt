--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: a write to an unconstructed DateInterval is a refused dynamic property (PHL half of the twin pair)
--SKIPIF--
<?php
if (function_exists('zend_version')) {
    echo "skip PHL-pinned half of the twin pair";
}
?>
--FILE--
<?php
/* An object with no struct has no property table either, so php's write handler
 * has nothing to convert and the standard one takes over: `$i->y = 1.5` on an
 * unconstructed interval CREATES a deprecated dynamic property holding the raw
 * 1.5, which the constructor then overwrites with the struct's 0. The scope policy refuses a
 * deprecation and PHL refuses a dynamic property outright, so every one of these
 * names meets the Error PHL raises for any other undeclared write.
 *
 * The read side is not this pair's business: it is php-exact and pinned in
 * 001-smoke/function/date_interval_period_lazy_props.phpt. */
$i = (new ReflectionClass('DateInterval'))->newInstanceWithoutConstructor();
foreach (['y', 'f', 'invert', 'days', 'from_string', 'nope'] as $p) {
    try {
        $i->$p = 1.5;
        echo "$p written: ", var_export($i->$p, true), "\n";
    } catch (Error $e) {
        echo get_class($e), ': ', $e->getMessage(), "\n";
    }
}
/* Nothing landed, so the object is still the empty one it was. */
var_dump(get_object_vars($i));

/* An increment reads before it writes, and the read miss comes first. */
try {
    $i->d++;
} catch (Error $e) {
    echo get_class($e), ': ', $e->getMessage(), "\n";
}

/* The constructor still installs the whole set afterwards. */
$i->__construct('P1D');
echo implode(',', array_keys(get_object_vars($i))), "\n";
$i->y = 1.5;
var_dump($i->y);
?>
--EXPECT--
Error: Cannot create dynamic property DateInterval::$y
Error: Cannot create dynamic property DateInterval::$f
Error: Cannot create dynamic property DateInterval::$invert
Error: Cannot create dynamic property DateInterval::$days
Error: Cannot create dynamic property DateInterval::$from_string
Error: Cannot create dynamic property DateInterval::$nope
array(0) {
}
Error: Cannot create dynamic property DateInterval::$d
y,m,d,h,i,s,f,invert,days,from_string
int(1)
