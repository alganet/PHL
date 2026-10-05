--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument to a constructor is refused where it is sent
--FILE--
<?php
/* php resolves a `new`'s class before its arguments run, and a NAME at the send
 * of its own argument against that class's constructor: an unknown name, or one
 * a positional argument already filled, throws before a LATER argument runs and
 * before a plain variable operand is read. A class that declares no constructor
 * takes no name at all. */
namespace N;
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
spl_autoload_register(function ($c) {
    echo "  autoload $c\n";
    if ($c === 'N\Lazy') { eval('namespace N; class Lazy { function __construct($a = 0) {} }'); }
});
function s($t) { echo "  ran $t\n"; return 1; }
function t($label, $c) {
    echo "$label\n";
    try { $r = $c(); echo "  = ", is_object($r) ? get_class($r) : var_export($r, true), "\n"; }
    catch (\Error $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
class B {
    function __construct(public $a = 0, &$r = null) {}
    static function mkStatic() { return new static(zz: $u, a: s('static')); }
    static function mkSelf() { return new self(1, a: $u, b: s('self')); }
}
class K extends B { function mkParent() { return new parent(zz: $u, a: s('parent')); } }
class V { function __construct($a, ...$rest) { echo "  rest ", json_encode($rest), "\n"; } }
class P { private function __construct($a = 0) {} }
class D {}
$arr = [];
$o = new B;
$n = 'N\B';
t('unknown', fn() => new B(zz: $u, a: s('unknown')));
t('known', fn() => new B(a: 2)->a);
t('overwrites', fn() => new B(1, a: $u, b: s('overwrites')));
t('overwrites after an unread element', fn() => new B($arr['k'], a: $u, b: s('element')));
t('new static', fn() => K::mkStatic());
t('new self', fn() => B::mkSelf());
t('new parent', fn() => (new K)->mkParent());
t('from an object', fn() => new $o(zz: $u, a: s('object')));
t('from a string', fn() => new $n(zz: $u, a: s('string')));
t('after a by-reference element', fn() => new \N\B(r: $arr[0], zz: $u, a: s('by-ref')));
t('variadic collects it', fn() => new V(1, zz: 3, q: 2));
t('variadic overwrites', fn() => new V(1, a: $u));
t('private constructor first', fn() => new P(zz: $u, a: s('private')));
t('no constructor', fn() => new D(1, zz: $u, a: s('no constructor')));
t('autoloaded', fn() => new Lazy(zz: $u, a: s('autoloaded')));
t('built-in constructor', fn() => new \SplFixedArray(zz: $u, size: s('built-in')));
t('built-in overwrites', fn() => new \ArrayObject([], 0, iteratorClass: 'x', flags: s('built-in')));
t('built-in reordered', fn() => new \ArrayObject(flags: 2, array: [1])->getFlags());
t('exception', fn() => new \Exception(message: 'm', zz: $u, code: s('exception')));
var_dump($arr);
--EXPECT--
unknown
  Error: Unknown named parameter $zz
known
  = 2
overwrites
  Error: Named parameter $a overwrites previous argument
overwrites after an unread element
  warning: Undefined array key "k"
  Error: Named parameter $a overwrites previous argument
new static
  Error: Unknown named parameter $zz
new self
  Error: Named parameter $a overwrites previous argument
new parent
  Error: Unknown named parameter $zz
from an object
  Error: Unknown named parameter $zz
from a string
  Error: Unknown named parameter $zz
after a by-reference element
  Error: Unknown named parameter $zz
variadic collects it
  rest {"zz":3,"q":2}
  = N\V
variadic overwrites
  Error: Named parameter $a overwrites previous argument
private constructor first
  Error: Call to private N\P::__construct() from global scope
no constructor
  Error: Unknown named parameter $zz
autoloaded
  autoload N\Lazy
  Error: Unknown named parameter $zz
built-in constructor
  Error: Unknown named parameter $zz
built-in overwrites
  ran built-in
  Error: Named parameter $flags overwrites previous argument
built-in reordered
  = 2
exception
  Error: Unknown named parameter $zz
array(0) {
}
