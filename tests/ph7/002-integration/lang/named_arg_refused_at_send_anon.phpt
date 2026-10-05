--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A named argument to an anonymous class's constructor is refused where it is sent
--FILE--
<?php
/* php declares an anonymous class before its constructor arguments run, and
 * resolves a NAME at the send of its own argument against that constructor: an
 * unknown name, or one a positional argument already filled, throws before a
 * LATER argument runs and before a plain variable operand is read. A plain
 * variable is read by its send, so a by-reference constructor parameter creates
 * it and a by-value one warns once. */
namespace N;
set_error_handler(function ($no, $msg) { echo "  warning: $msg\n"; return true; });
spl_autoload_register(function ($c) {
    echo "  autoload $c\n";
    if ($c === 'N\Lazy') { eval('namespace N; class Lazy { function __construct($a = 0) {} }'); }
});
function s($t) { echo "  ran $t\n"; return 1; }
function t($label, $c) {
    echo "$label\n";
    try { $r = $c(); echo "  = ", is_object($r) ? (new \ReflectionClass($r))->isAnonymous() ? 'anonymous' : get_class($r) : var_export($r, true), "\n"; }
    catch (\Error $e) { echo "  ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
class B { function __construct(public $a = 0, public $b = 0) {} }
$arr = [1];
t('unknown', fn() => new class(zz: $u, a: s('unknown')) { function __construct($a = 0) {} });
t('known', fn() => (new class(b: 2, a: 3) { function __construct(public $a = 0, public $b = 0) {} })->b);
t('overwrites', fn() => new class(1, a: $u, b: s('overwrites')) { function __construct($a = 0, $b = 0) {} });
t('inherited constructor', fn() => new class(zz: $u, a: s('inherited')) extends B {});
t('no constructor', fn() => new class(1, zz: $u, a: s('no constructor')) {});
t('after an unpack', fn() => new class(...$arr, zz: $u, b: s('unpack')) { function __construct($a = 0, $b = 0) {} });
t('variadic collects it', fn() => new class(1, zz: 3, q: 2) { function __construct($a, ...$rest) { echo "  rest ", json_encode($rest), "\n"; } });
t('built-in parent', fn() => new class(zz: $u, array: s('built-in')) extends \ArrayObject {});
t('autoloaded parent', fn() => new class(zz: $u, a: s('autoloaded')) extends Lazy {});
t('element read before the refusal', fn() => new class(s('element'), zz: $arr['k']) { function __construct($a = 0) {} });
echo "by reference\n";
new class($r1) { function __construct(&$x) { $x = 5; } };
new class(x: $r2) { function __construct(&$x) { $x = 6; } };
var_dump($r1, $r2);
echo "by value\n";
new class($v1) { function __construct($x) { var_dump($x); } };
new class(x: $v2) { function __construct($x) { var_dump($x); } };
echo "a later argument writes the variable\n";
$w = 1;
new class($w, $w = 5) { function __construct($x, $y) { var_dump($x, $y); } };
new class(x: $w, y: $w .= '!') { function __construct($x, $y) { var_dump($x, $y); } };
new class($w, $w = 9) { function __construct(&$x, $y) { var_dump($x); } };
var_dump($w);
--EXPECT--
unknown
  Error: Unknown named parameter $zz
known
  = 2
overwrites
  Error: Named parameter $a overwrites previous argument
inherited constructor
  Error: Unknown named parameter $zz
no constructor
  Error: Unknown named parameter $zz
after an unpack
  Error: Unknown named parameter $zz
variadic collects it
  rest {"zz":3,"q":2}
  = anonymous
built-in parent
  Error: Unknown named parameter $zz
autoloaded parent
  autoload N\Lazy
  Error: Unknown named parameter $zz
element read before the refusal
  ran element
  warning: Undefined array key "k"
  Error: Unknown named parameter $zz
by reference
int(5)
int(6)
by value
  warning: Undefined variable $v1
NULL
  warning: Undefined variable $v2
NULL
a later argument writes the variable
int(1)
int(5)
int(5)
string(2) "5!"
int(9)
int(9)
