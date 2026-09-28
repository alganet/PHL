--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A private STATIC is inherited, a private CONSTANT is not
--DESCRIPTION--
The two other private member kinds go opposite ways and this engine had them
exactly backwards: it copied every constant down regardless of visibility and
skipped every private static. php keeps a private static in the child's property
table -- so the child's name answers with the visibility refusal, and a base
method's `static::$s` with the child as its late-static-binding target finds its
own static -- while a private constant is not reachable by the child's name at all.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo "[diag] $m\n"; return true; });
class PrivStatBase {
    private static $s = 1;
    private const K = 'base-const';
    protected static $ps = 2;
    public static function selfPair()   { return [self::$s, self::K]; }
    public static function staticStat()  { return static::$s; }
    public static function staticConst() { return static::K; }
}
class PrivStatChild extends PrivStatBase {}

function priv_stat_try($tag, callable $fn) {
    echo $tag, ': ';
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ': ', $e->getMessage(), "\n"; }
}

/* A private STATIC comes down: php keeps it in the child's property table, so the
 * child's name answers with the VISIBILITY refusal and not the undeclared-static
 * one -- and `static::$s` from a base method with the child as its
 * late-static-binding target finds its own static. */
priv_stat_try('child static ', fn() => PrivStatChild::$s);
priv_stat_try('own static   ', fn() => PrivStatBase::$s);
priv_stat_try('self pair    ', fn() => PrivStatChild::selfPair());
priv_stat_try('lsb static   ', fn() => PrivStatChild::staticStat());

/* A private CONSTANT does NOT: php answers the child's name with "Undefined
 * constant", never with the visibility refusal it words for the declaring class. */
priv_stat_try('child const  ', fn() => PrivStatChild::K);
priv_stat_try('own const    ', fn() => PrivStatBase::K);
priv_stat_try('lsb const    ', fn() => PrivStatChild::staticConst());
priv_stat_try('constant()   ', fn() => constant('PrivStatChild::K'));
var_dump(defined('PrivStatChild::K'), defined('PrivStatBase::K'));

/* Neither is on the child's listing surfaces; the protected static still is. */
var_dump(array_keys((new ReflectionClass('PrivStatChild'))->getStaticProperties()));
var_dump(array_keys((new ReflectionClass('PrivStatChild'))->getConstants()));
var_dump((new ReflectionClass('PrivStatChild'))->hasConstant('K'));
var_dump(property_exists('PrivStatChild', 's'), property_exists('PrivStatChild', 'ps'));

/* Reached through an INSTANCE, the inherited private static is simply not there. */
priv_stat_try('inst child   ', fn() => (new PrivStatChild)->s);
priv_stat_try('inst own     ', fn() => (new PrivStatBase)->s);
restore_error_handler();
?>
--EXPECT--
child static : Error: Cannot access private property PrivStatChild::$s
own static   : Error: Cannot access private property PrivStatBase::$s
self pair    : array(2) {
  [0]=>
  int(1)
  [1]=>
  string(10) "base-const"
}
lsb static   : int(1)
child const  : Error: Undefined constant PrivStatChild::K
own const    : Error: Cannot access private constant PrivStatBase::K
lsb const    : Error: Undefined constant PrivStatChild::K
constant()   : Error: Undefined constant PrivStatChild::K
bool(false)
bool(false)
array(1) {
  [0]=>
  string(2) "ps"
}
array(0) {
}
bool(false)
bool(false)
bool(true)
inst child   : [diag] Undefined property: PrivStatChild::$s
NULL
inst own     : Error: Cannot access private property PrivStatBase::$s
