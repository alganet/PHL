--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A callable leaning on the calling scope raises php 8.2's deprecation at every door that checks it
--DESCRIPTION--
`'self::m'`, `['parent','m']` and `'static::m'` raise `Use of "self" in callables is
deprecated`, and an array callable whose method half is itself qualified raises
`Callables of the form ["C", "parent::m"] are deprecated`, naming the target's class
(the keyword inside it says nothing of its own). Each door raises it once whether or
not the method exists: is_callable(), every callback parameter, a user `callable`
declaration, Closure::fromCallable(). No scope to resolve against, is_callable()'s
syntax-only mode, and a `callable|string` declaration that takes the string first
raise nothing. The handler is the portable channel: this box's CLI masks deprecations.
--FILE--
<?php
set_error_handler(function ($n, $m) { echo "  [$n] $m\n"; return true; });
class CsdA { static function a() { return 'CsdA::a'; } }
class CsdP extends CsdA { static function p() { return 'CsdP::p'; } function i() { return 'CsdP::i'; } }
class CsdC extends CsdP {
    static function s() { return 'CsdC::s'; }
    static function h(...$a) { return true; }
    static function k($a, $b) { return $a <=> $b; }
    static function ob($b) { return strtoupper($b); }
    function m() { return 'm'; }
    function takes(callable $c) { return 'took'; }
    function takesOr(callable|string $c) { return 'took'; }
    function gives($c): callable { return $c; }
    function run() {
        $cases = [
            'is_callable self::s' => fn() => is_callable('self::s'),
            'is_callable [self,s]' => fn() => is_callable(['self', 's']),
            'is_callable parent::p' => fn() => is_callable('parent::p'),
            'is_callable [static,s]' => fn() => is_callable(['static', 's']),
            'is_callable self::nope' => fn() => is_callable('self::nope'),
            'is_callable [C,parent::p]' => fn() => is_callable(['CsdC', 'parent::p']),
            'is_callable [$this,parent::p]' => fn() => is_callable([$this, 'parent::p']),
            'is_callable [self,parent::p]' => fn() => is_callable(['self', 'parent::p']),
            'is_callable [$this,CsdA::a]' => fn() => is_callable([$this, 'CsdA::a']),
            'is_callable [$this,X::a]' => fn() => is_callable([$this, 'CsdX::a']),
            'is_callable [A,CsdC::s]' => fn() => is_callable([new CsdA, 'CsdC::s']),
            'is_callable syntax-only' => fn() => is_callable('self::s', true),
            'is_callable plain C::s' => fn() => is_callable('CsdC::s'),
            'call_user_func' => fn() => call_user_func('self::s'),
            'call_user_func [$this,parent::p]' => fn() => call_user_func([$this, 'parent::p']),
            'call_user_func_array' => fn() => call_user_func_array(['parent', 'p'], []),
            'forward_static_call' => fn() => forward_static_call('static::s'),
            'fromCallable' => fn() => (Closure::fromCallable('self::s'))(),
            'array_map' => fn() => implode(',', array_map('self::s', [1])),
            'array_filter' => fn() => count(array_filter([1], 'self::h')),
            'usort' => function () { $a = [2, 1]; usort($a, ['self', 'k']); return implode(',', $a); },
            'uksort' => function () { $a = [2 => 0, 1 => 0]; uksort($a, 'self::k'); return implode(',', array_keys($a)); },
            'array_udiff' => fn() => count(array_udiff([1], [2], 'self::k')),
            'preg_replace_callback' => fn() => preg_replace_callback('/a/', 'self::s', 'a'),
            'iterator_apply' => fn() => iterator_apply(new ArrayIterator([1]), 'self::h'),
            'set_error_handler' => function () { $o = set_error_handler('self::h'); set_error_handler($o); return 'set'; },
            'set_exception_handler' => function () { set_exception_handler('self::h'); restore_exception_handler(); return 'set'; },
            'spl_autoload_(un)register' => function () { spl_autoload_register('self::h'); return spl_autoload_unregister('self::h'); },
            'ob_start' => function () { ob_start('self::ob'); echo "x"; ob_end_flush(); return ''; },
            'CallbackFilterIterator' => fn() => get_class(new CallbackFilterIterator(new ArrayIterator([]), 'self::h')),
            'callable parameter' => fn() => $this->takes('self::s'),
            'callable|string parameter' => fn() => $this->takesOr('self::s'),
            'callable return' => fn() => is_string($this->gives('parent::p')),
            'dynamic call (none)' => function () { try { $f = 'self::s'; return $f(); } catch (Error $e) { return $e->getMessage(); } },
        ];
        foreach ($cases as $label => $f) {
            echo $label, "\n";
            $r = $f();
            echo "  = ", var_export($r, true), "\n";
        }
    }
    static function fromStatic() {
        echo "static scope\n";
        var_dump(is_callable('self::m'), is_callable('parent::i'));
    }
}
(new CsdC)->run();
CsdC::fromStatic();
echo "global scope\n";
var_dump(is_callable('self::s'), is_callable(['parent', 'p']), is_callable('static::s'));
class CsdN { function t() { return is_callable('parent::x'); } }
echo "no parent\n";
var_dump((new CsdN)->t());
restore_error_handler();
?>
--EXPECT--
is_callable self::s
  [8192] Use of "self" in callables is deprecated
  = true
is_callable [self,s]
  [8192] Use of "self" in callables is deprecated
  = true
is_callable parent::p
  [8192] Use of "parent" in callables is deprecated
  = true
is_callable [static,s]
  [8192] Use of "static" in callables is deprecated
  = true
is_callable self::nope
  [8192] Use of "self" in callables is deprecated
  = false
is_callable [C,parent::p]
  [8192] Callables of the form ["CsdC", "parent::p"] are deprecated
  = true
is_callable [$this,parent::p]
  [8192] Callables of the form ["CsdC", "parent::p"] are deprecated
  = true
is_callable [self,parent::p]
  [8192] Use of "self" in callables is deprecated
  [8192] Callables of the form ["CsdC", "parent::p"] are deprecated
  = true
is_callable [$this,CsdA::a]
  [8192] Callables of the form ["CsdC", "CsdA::a"] are deprecated
  = true
is_callable [$this,X::a]
  = false
is_callable [A,CsdC::s]
  = false
is_callable syntax-only
  = true
is_callable plain C::s
  = true
call_user_func
  [8192] Use of "self" in callables is deprecated
  = 'CsdC::s'
call_user_func [$this,parent::p]
  [8192] Callables of the form ["CsdC", "parent::p"] are deprecated
  = 'CsdP::p'
call_user_func_array
  [8192] Use of "parent" in callables is deprecated
  = 'CsdP::p'
forward_static_call
  [8192] Use of "static" in callables is deprecated
  = 'CsdC::s'
fromCallable
  [8192] Use of "self" in callables is deprecated
  = 'CsdC::s'
array_map
  [8192] Use of "self" in callables is deprecated
  = 'CsdC::s'
array_filter
  [8192] Use of "self" in callables is deprecated
  = 1
usort
  [8192] Use of "self" in callables is deprecated
  = '1,2'
uksort
  [8192] Use of "self" in callables is deprecated
  = '1,2'
array_udiff
  [8192] Use of "self" in callables is deprecated
  = 1
preg_replace_callback
  [8192] Use of "self" in callables is deprecated
  = 'CsdC::s'
iterator_apply
  [8192] Use of "self" in callables is deprecated
  = 1
set_error_handler
  [8192] Use of "self" in callables is deprecated
  = 'set'
set_exception_handler
  [8192] Use of "self" in callables is deprecated
  = 'set'
spl_autoload_(un)register
  [8192] Use of "self" in callables is deprecated
  [8192] Use of "self" in callables is deprecated
  = true
ob_start
  [8192] Use of "self" in callables is deprecated
X  = ''
CallbackFilterIterator
  [8192] Use of "self" in callables is deprecated
  = 'CallbackFilterIterator'
callable parameter
  [8192] Use of "self" in callables is deprecated
  = 'took'
callable|string parameter
  = 'took'
callable return
  [8192] Use of "parent" in callables is deprecated
  = true
dynamic call (none)
  = 'Class "self" not found'
static scope
  [8192] Use of "self" in callables is deprecated
  [8192] Use of "parent" in callables is deprecated
bool(false)
bool(false)
global scope
bool(false)
bool(false)
bool(false)
no parent
bool(false)
