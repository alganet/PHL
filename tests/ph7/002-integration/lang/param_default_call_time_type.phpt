--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: an unfolded default is held to its type at the call
--FILE--
<?php
// A default the compiler cannot fold is held to the parameter's type at the
// call, exactly like a passed argument: coerced in the caller's mode, refused
// with the ordinary TypeError naming the call's line.
const PDCT_S = "a";
const PDCT_N = "5";
const PDCT_Z = null;
function pdct_show(callable $c) {
    try { var_dump($c()); }
    catch (TypeError $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
function pdct_str(int $x = PDCT_S) { return $x; }
function pdct_num(int $x = PDCT_N) { return $x; }
function pdct_obj(int $x = new stdClass) { return $x; }
function pdct_null(int $x = PDCT_Z) { return $x; }
function pdct_nullable(?int $x = PDCT_Z) { return $x; }
function pdct_tostr(string $x = new ArrayObject) { return $x; }
function pdct_union(float|string $x = 1) { return $x; }
function pdct_second(int $a = 1, int $b = PDCT_S) { return $b; }
class PdctA {
    const C = "x";
    function m(Countable $c = new stdClass) { return $c; }
    static function s(int $x = self::C) { return $x; }
}
function pdct_gen(int $x = PDCT_S) { yield $x; }
pdct_show(fn() => pdct_str());
pdct_show(fn() => pdct_num());
pdct_show(fn() => pdct_obj());
pdct_show(fn() => pdct_null());
pdct_show(fn() => pdct_nullable());
pdct_show(fn() => pdct_tostr());
pdct_show(fn() => pdct_union());
pdct_show(fn() => pdct_second(a: 2));
pdct_show(fn() => (new PdctA)->m());
pdct_show(fn() => PdctA::s());
pdct_show(fn() => pdct_gen()->current());
pdct_show(fn() => call_user_func('pdct_str'));
--EXPECTF--
TypeError: pdct_str(): Argument #1 ($x) must be of type int, string given, called in %s on line 26
int(5)
TypeError: pdct_obj(): Argument #1 ($x) must be of type int, stdClass given, called in %s on line 28
TypeError: pdct_null(): Argument #1 ($x) must be of type int, null given, called in %s on line 29
NULL
TypeError: pdct_tostr(): Argument #1 ($x) must be of type string, ArrayObject given, called in %s on line 31
float(1)
TypeError: pdct_second(): Argument #2 ($b) must be of type int, string given, called in %s on line 33
TypeError: PdctA::m(): Argument #1 ($c) must be of type Countable, stdClass given, called in %s on line 34
TypeError: PdctA::s(): Argument #1 ($x) must be of type int, string given, called in %s on line 35
TypeError: pdct_gen(): Argument #1 ($x) must be of type int, string given, called in %s on line 36
TypeError: pdct_str(): Argument #1 ($x) must be of type int, string given, called in %s on line 37
