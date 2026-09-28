--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An anonymous class's constructor arguments are an ordinary argument list
--DESCRIPTION--
`new class(args) { … }` cannot be parsed by the expression parser's argument machinery — the
class BODY sits between the parentheses and the rest of the expression — so PH7_CompileAnnonClass
compiles that list from raw tokens, and it recognized neither of the two forms the ordinary path
does. A SPREAD was compiled as one ordinary argument, so `new class(...$a) {}` passed the ARRAY
where php passes its elements (and `Array to string conversion` where php passes nothing at all),
and a NAMED argument was `Syntax error: Unexpected token ':'` on source php compiles. Both work
now, through the same OP_SPREAD and the same VmCallArgMap the ordinary call emits, and the four
compile-time ORDER rules php enforces on any argument list are enforced here too.
--FILE--
<?php
function t($label, $fn) {
    echo "== $label\n";
    try { var_export($fn()); echo "\n"; } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}
class Base { public $got; function __construct($a = 0, $b = 0, $c = 0) { $this->got = "$a|$b|$c"; } }

echo "-> named arguments\n";
t('by name',            fn() => (new class(b: 5) extends Base {})->got);
t('two names',          fn() => (new class(c: 3, a: 1) extends Base {})->got);
t('trailing comma',     fn() => (new class(b: 4,) extends Base {})->got);
t('reserved word name', function () {
    $o = new class(class: 5) { public $v; function __construct($class = 0) { $this->v = $class; } };
    return $o->v;
});
t('unknown name',       fn() => (new class(q: 1) extends Base {})->got);
t('duplicate name',     fn() => (new class(a: 1, a: 2) extends Base {})->got);

echo "-> unpacking\n";
t('spread literal',     fn() => (new class(...[1, 2, 3]) extends Base {})->got);
t('spread variable',    function () { $a = [7, 8]; return (new class(...$a) extends Base {})->got; });
t('positional + spread', function () { $a = [2, 3]; return (new class(1, ...$a) extends Base {})->got; });
t('spread + named',     fn() => (new class(...[1], c: 9) extends Base {})->got);
t('spread of names',    fn() => (new class(...['c' => 9], ...['a' => 1]) extends Base {})->got);
t('spread Traversable', fn() => (new class(...new ArrayIterator(['c' => 7])) extends Base {})->got);
t('spread generator',   function () {
    $g = (function () { yield 'c' => 7; })();
    return (new class(...$g) extends Base {})->got;
});
t('spread bad key',     function () {
    $g = (function () { yield 1.5 => 7; })();
    return (new class(...$g) extends Base {})->got;
});
t('spread non-array',   fn() => (new class(...'str') extends Base {})->got);
t('spread by reference', function () {
    $a = [1];
    $o = new class(...$a) { function __construct(&$r) { $r = 42; } };
    return $a;
});

echo "-> the list is still an expression list\n";
t('nested calls',   fn() => (new class(strlen('abcd'), max(1, 9)) extends Base {})->got);
t('class constant', fn() => (new class(Base::class) extends Base {})->got);
t('nested anon',    fn() => (new class(new class(5) extends Base {}) {
    public $v; function __construct($i) { $this->v = $i->got; }
})->v);
t('per-site class', function () { $f = fn($n) => new class($n) extends Base {}; return [$f(1)->got, $f(2)->got]; });
?>
--EXPECT--
-> named arguments
== by name
'0|5|0'
== two names
'1|0|3'
== trailing comma
'0|4|0'
== reserved word name
5
== unknown name
Error: Unknown named parameter $q
== duplicate name
Error: Named parameter $a overwrites previous argument
-> unpacking
== spread literal
'1|2|3'
== spread variable
'7|8|0'
== positional + spread
'1|2|3'
== spread + named
'1|0|9'
== spread of names
'1|0|9'
== spread Traversable
'0|0|7'
== spread generator
'0|0|7'
== spread bad key
Error: Keys must be of type int|string during argument unpacking
== spread non-array
TypeError: Only arrays and Traversables can be unpacked, string given
== spread by reference
array (
  0 => 42,
)
-> the list is still an expression list
== nested calls
'4|9|0'
== class constant
'Base|0|0'
== nested anon
'5|0|0'
== per-site class
array (
  0 => '1|0|0',
  1 => '2|0|0',
)
