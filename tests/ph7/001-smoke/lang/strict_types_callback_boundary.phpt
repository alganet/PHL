--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An internal function's callback binds weakly; call_user_func forwards the caller's mode
--FILE--
<?php
declare(strict_types=1);
function stcbTakesInt(int $i) { return $i; }
function stcbTry(callable $f) {
    try { $r = $f(); echo "ok "; var_dump($r); }
    catch (\TypeError $e) { echo get_class($e), ": ", preg_replace('/, called in .*/', '', $e->getMessage()), "\n"; }
}

// an INTERNAL function reaching for a callback has no calling file: weak
stcbTry(fn() => array_map('stcbTakesInt', ["5"])[0]);
stcbTry(fn() => array_map('strlen', [5])[0]);
stcbTry(fn() => (new ReflectionFunction('stcbTakesInt'))->invoke("5"));
stcbTry(function () { $a = [2, 1]; usort($a, fn(int $x, int $y) => $x <=> $y); return $a[0]; });
stcbTry(fn() => preg_replace_callback('/\d/', fn(array $m) => $m[0], "5"));

// call_user_func and call_user_func_array are php's two FORWARDS: strict
stcbTry(fn() => call_user_func('stcbTakesInt', "5"));
stcbTry(fn() => call_user_func_array('stcbTakesInt', ["5"]));
stcbTry(fn() => call_user_func('strlen', 5));
stcbTry(fn() => call_user_func_array('strlen', [5]));
// and they accept what strict mode accepts
stcbTry(fn() => call_user_func('stcbTakesInt', 5));
stcbTry(fn() => call_user_func('strlen', "abc"));

// a call WRITTEN in this file is strict wherever it sits, including inside a callback
stcbTry(fn() => array_map(fn($v) => stcbTakesInt($v), ["5"])[0]);
// a variable call and a first-class callable are written here too
stcbTry(function () { $f = 'strlen'; return $f(5); });
stcbTry(function () { $c = strlen(...); return $c(5); });
?>
--EXPECT--
ok int(5)
ok int(1)
ok int(5)
ok int(1)
ok string(1) "5"
TypeError: stcbTakesInt(): Argument #1 ($i) must be of type int, string given
TypeError: stcbTakesInt(): Argument #1 ($i) must be of type int, string given
TypeError: strlen(): Argument #1 ($string) must be of type string, int given
TypeError: strlen(): Argument #1 ($string) must be of type string, int given
ok int(5)
ok int(3)
TypeError: stcbTakesInt(): Argument #1 ($i) must be of type int, string given
TypeError: strlen(): Argument #1 ($string) must be of type string, int given
TypeError: strlen(): Argument #1 ($string) must be of type string, int given
