--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A scope keyword in a callable's class half resolves in any case
--FILE--
<?php
// A scope keyword in a callable's class half is folded before it is compared, so
// 'SELF::s', ['Static','s'] and constant('Parent::K') answer as the lower-case
// spelling does, and every refusal names the keyword in lower case.
set_error_handler(function ($n, $s) { echo "E$n: $s\n"; return true; });
function csk_show(callable|string|array $f) {
    try { var_dump($f()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
foreach (['SELF::s', ['Parent', 's'], 'Static::s'] as $c) {
    csk_show(fn() => call_user_func($c));
    csk_show(fn() => is_callable($c));
    $k = (is_array($c) ? $c[0] : substr($c, 0, strpos($c, ':'))) . '::K';
    csk_show(fn() => constant($k));
    csk_show(fn() => defined($k));
}
class CskP { const K = 'CskP::K'; static function s() { return 'CskP::s from ' . static::class; } }
class CskA extends CskP {
    const K = 'CskA::K';
    static function s() { return 'CskA::s from ' . static::class; }
    function t() {
        foreach (['SELF::s', 'Self::s', 'STATIC::s', 'PARENT::s', ['Parent', 's'], ['sElF', 's']] as $c) {
            echo "== ", is_array($c) ? implode(',', $c) : $c, "\n";
            csk_show(fn() => call_user_func($c));
            csk_show(fn() => call_user_func_array($c, []));
            csk_show(fn() => is_callable($c));
            csk_show(fn() => Closure::fromCallable($c)());
            csk_show(fn() => array_map($c, [1])[0]);
            csk_show(fn() => call_user_func([$this, is_array($c) ? implode('::', $c) : $c]));
        }
        foreach (['SELF', 'Static', 'PARENT'] as $k) {
            csk_show(fn() => constant("$k::K"));
            csk_show(fn() => defined("$k::K"));
        }
    }
}
class CskB extends CskA {}
(new CskB)->t();
class CskQ { static function s() { return 'CskQ::s'; }
    function t() {
        foreach (['PARENT::s', ['Parent', 's'], 'parent::s'] as $c) {
            csk_show(fn() => call_user_func($c));
            csk_show(fn() => is_callable($c));
            csk_show(fn() => Closure::fromCallable($c));
        }
        csk_show(fn() => constant('Parent::K'));
        $f = new Fiber('SELF::s');
        var_dump($f->start(), $f->getReturn());
    }
}
(new CskQ)->t();
restore_error_handler();
--EXPECT--
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "self" when no class scope is active
bool(false)
Error: Cannot access "self" when no class scope is active
Error: Cannot access "self" when no class scope is active
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "parent" when no class scope is active
bool(false)
Error: Cannot access "parent" when no class scope is active
Error: Cannot access "parent" when no class scope is active
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "static" when no class scope is active
bool(false)
Error: Cannot access "static" when no class scope is active
Error: Cannot access "static" when no class scope is active
== SELF::s
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
bool(true)
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Callables of the form ["CskB", "SELF::s"] are deprecated
string(17) "CskA::s from CskB"
== Self::s
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
bool(true)
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Callables of the form ["CskB", "Self::s"] are deprecated
string(17) "CskA::s from CskB"
== STATIC::s
E8192: Use of "static" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "static" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "static" in callables is deprecated
bool(true)
E8192: Use of "static" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "static" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Callables of the form ["CskB", "STATIC::s"] are deprecated
string(17) "CskA::s from CskB"
== PARENT::s
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Use of "parent" in callables is deprecated
bool(true)
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Callables of the form ["CskB", "PARENT::s"] are deprecated
string(17) "CskA::s from CskB"
== Parent,s
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Use of "parent" in callables is deprecated
bool(true)
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Use of "parent" in callables is deprecated
string(17) "CskP::s from CskB"
E8192: Callables of the form ["CskB", "Parent::s"] are deprecated
string(17) "CskA::s from CskB"
== sElF,s
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
bool(true)
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Use of "self" in callables is deprecated
string(17) "CskA::s from CskB"
E8192: Callables of the form ["CskB", "sElF::s"] are deprecated
string(17) "CskA::s from CskB"
string(7) "CskA::K"
bool(true)
string(7) "CskA::K"
bool(true)
string(7) "CskP::K"
bool(true)
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "parent" when current class scope has no parent
bool(false)
TypeError: Failed to create closure from callable: cannot access "parent" when current class scope has no parent
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "parent" when current class scope has no parent
bool(false)
TypeError: Failed to create closure from callable: cannot access "parent" when current class scope has no parent
TypeError: call_user_func(): Argument #1 ($callback) must be a valid callback, cannot access "parent" when current class scope has no parent
bool(false)
TypeError: Failed to create closure from callable: cannot access "parent" when current class scope has no parent
Error: Cannot access "parent" when current class scope has no parent
E8192: Use of "self" in callables is deprecated
NULL
string(7) "CskQ::s"
