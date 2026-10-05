--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument's name is refused where that argument is sent
--FILE--
<?php
/* php resolves a NAME at the send of its own argument, so an unknown name, or
 * one a positional argument already filled, throws before any LATER argument
 * runs. The argument's own expression has run by then (a call, a subscript, a
 * property read), but a plain variable is read by the send itself, so an
 * undefined one never warns. Arguments sent before it were bound already: a
 * by-value undefined variable among them has warned. */
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
function nas_s($t) { echo "  ran $t\n"; return 1; }
function nas_f($a = 1, $b = 2) { return "a=$a b=$b"; }
function nas_v($a, ...$rest) { return json_encode([$a, $rest]); }
class NasK {
    function m($a = 1) { return "m a=$a"; }
    static function sm($a = 1) { return "sm a=$a"; }
}
function nas_t($label, $c) {
    echo "$label\n";
    try { $r = $c(); echo "  = $r\n"; }
    catch (Error $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
$arr = [];
$o = new stdClass;
$k = new NasK;
$n = 'nas_f';
nas_t('undefined var, then a call', fn() => nas_f(zz: $undef, b: nas_s('b')));
nas_t('missing key', fn() => nas_f(zz: $arr['nokey'], b: nas_s('b')));
nas_t('missing property', fn() => nas_f(zz: $o->nope, b: nas_s('b')));
nas_t('a call as the value', fn() => nas_f(zz: nas_s('zz'), b: nas_s('b')));
nas_t('after a positional', fn() => nas_f(nas_s('pos'), zz: nas_s('zz')));
nas_t('named after named', fn() => nas_f(b: nas_s('b'), zz: $undef));
nas_t('overwrites, value a call', fn() => nas_f(1, a: nas_s('a'), b: nas_s('b')));
nas_t('overwrites, value a variable', fn() => nas_f(1, a: $undef));
nas_t('earlier by-value undefined', fn() => nas_f(a: $undef1, zz: $undef2));
nas_t('earlier positional undefined', fn() => nas_f($undef3, zz: 1));
nas_t('method', fn() => $k->m(zz: $undef, a: nas_s('a')));
nas_t('static method', fn() => NasK::sm(zz: $undef, a: nas_s('a')));
nas_t('literal string callee', fn() => 'nas_f'(zz: $undef, b: nas_s('b')));
nas_t('variable callee', fn() => $n(zz: $undef, b: nas_s('b')));
nas_t('after an unpack', fn() => nas_f(...[1], zz: $undef));
nas_t('variadic takes it', fn() => nas_v(1, zz: nas_s('zz'), yy: 2));
nas_t('good names', fn() => nas_f(b: nas_s('b'), a: nas_s('a')));
--EXPECT--
undefined var, then a call
  Error: Unknown named parameter $zz
missing key
  warning: Undefined array key "nokey"
  Error: Unknown named parameter $zz
missing property
  warning: Undefined property: stdClass::$nope
  Error: Unknown named parameter $zz
a call as the value
  ran zz
  Error: Unknown named parameter $zz
after a positional
  ran pos
  ran zz
  Error: Unknown named parameter $zz
named after named
  ran b
  Error: Unknown named parameter $zz
overwrites, value a call
  ran a
  Error: Named parameter $a overwrites previous argument
overwrites, value a variable
  Error: Named parameter $a overwrites previous argument
earlier by-value undefined
  warning: Undefined variable $undef1
  Error: Unknown named parameter $zz
earlier positional undefined
  warning: Undefined variable $undef3
  Error: Unknown named parameter $zz
method
  Error: Unknown named parameter $zz
static method
  Error: Unknown named parameter $zz
literal string callee
  Error: Unknown named parameter $zz
variable callee
  Error: Unknown named parameter $zz
after an unpack
  Error: Unknown named parameter $zz
variadic takes it
  ran zz
  = [1,{"zz":1,"yy":2}]
good names
  ran b
  ran a
  = a=1 b=1
