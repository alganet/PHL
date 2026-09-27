--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
#[\NoDiscard] warns where the answer is dropped, and (void) says it was on purpose
--DESCRIPTION--
php 8.5's `#[\NoDiscard]` warns at the CALL when the caller throws the answer
away, and the `(void)` cast is the one thing that silences it. Neither existed
here: the attribute class was undefined and `(void) $x;` was a parse error, so
php's nine IMMUTABLE date mutators -- the single most common way to misuse
DateTimeImmutable, since `$d->modify('+1 day');` as a statement does nothing at
all -- said nothing.

The rules are php's. "Dropped" means the call's OWN result is unused, so
`f() + 1;` and `if (f())` are silent while `f();` is not, and php carries the
same bit through `call_user_func()` (a compile-time special case there, so the
literal name forwards and a variable holding it does not). `(void)` is a cast
TOKEN, not an expression operator: php's grammar takes it at the head of an
expression statement and at the head of each `for` clause element -- except the
condition's LAST element, which has to BE the condition -- and reports
`unexpected token "(void)"` anywhere else. The warning is E_USER_WARNING, the
subject is a "method" whenever the callee has a class scope (a closure declared
in a class body included), and the declaration itself is refused on a `void` or
`never` function and on a constructor.
--FILE--
<?php
$pre = "#[\\NoDiscard] function f(): int { return 1; }\n";
$cases = [
    'a bare call statement' => 'f();',
    'the answer assigned' => '$x = f();',
    'an intentional drop' => '(void) f();',
    'the cast folds case' => '(VOID) f();',
    'another cast consumes it' => '(int) f();',
    'an operator consumes it' => 'f() + 1;',
    'a condition consumes it' => 'if (f()) {}',
    'silence does not' => '@f();',
    'a default does not' => 'f() ?: 1;',
    'nor does a short-circuit' => 'true && f();',
    'nor an argument' => 'var_dump(f());',
    'nor an array element' => '[f()];',
    'nor a ternary arm' => 'true ? f() : 0;',
    'nor echo' => 'echo f(), "\n";',
    'every time round a loop' => 'for ($i = 0; $i < 2; $i++) { f(); }',
    'never, if never called' => 'function h() { f(); } echo "no call\n";',
    'through a variable name' => '$c = "f"; $c();',
    'through a first-class callable' => '$k = f(...); $k();',
    'not through array_map' => 'array_map("f", [1]);',
    'but through call_user_func' => 'call_user_func("f");',
    'and call_user_func_array' => 'call_user_func_array("f", []);',
    'not when THAT answer is used' => '$x = call_user_func("f");',
    'not when THAT one is cast' => '(void) call_user_func("f");',
    'and not through a variable' => '$g = "call_user_func"; $g("f");',
    'a method' => 'class C { #[\NoDiscard] public function m(): int { return 1; } } (new C)->m();',
    'a nullsafe one' => 'class C { #[\NoDiscard] public function m(): int { return 1; } } $o = new C; $o?->m();',
    'a static one' => 'class C { #[\NoDiscard] public static function s(): int { return 2; } } C::s();',
    'named by the DECLARING class' => 'class A { #[\NoDiscard] public function m(): int { return 1; } } class B extends A {} (new B)->m();',
    'parent:: too' => 'class A { #[\NoDiscard] public function m(): int { return 1; } } class B extends A { public function n() { parent::m(); } } (new B)->n();',
    'a trait method takes the using class' => 'trait T { #[\NoDiscard] public function m(): int { return 1; } } class C { use T; } (new C)->m();',
    '__invoke' => 'class C { #[\NoDiscard] public function __invoke(): int { return 1; } } $o = new C; $o();',
    'an array callable' => 'class C { #[\NoDiscard] public function m(): int { return 1; } } $cb = [new C, "m"]; $cb();',
    'a "C::m" string' => 'class C { #[\NoDiscard] public static function s(): int { return 2; } } $cb = "C::s"; $cb();',
    'a closure' => '$c = #[\NoDiscard] function(): int { return 1; }; $c();',
    'an arrow function' => '$c = #[\NoDiscard] fn(): int => 1; $c();',
    'a closure in a class body is a METHOD' => 'class C { public function go() { $c = #[\NoDiscard] function(): int { return 1; }; $c(); } } (new C)->go();',
    'the message is appended' => '#[\NoDiscard("because")] function w(): int { return 1; } w();',
    'by name as well' => '#[\NoDiscard(message: "why")] function w(): int { return 1; } w();',
    'null is no message' => '#[\NoDiscard(null)] function w(): int { return 1; } w();',
    'and it is a constant EXPRESSION' => 'const M = "cm"; #[\NoDiscard(M . "!")] function w(): int { return 1; } w();',
    'a handler sees E_USER_WARNING' => 'set_error_handler(function ($n, $s) { echo "[", $n, "] ", $s, "\n"; return true; }); f();',
    'and it lands before the body' => 'set_error_handler(function () { echo "[warn]\n"; return true; }); function tb(): int { echo "[body]\n"; return 1; } '
        . '$q = null; tb(); f();',
    'a void function may not claim it' => '#[\NoDiscard] function v(): void {}',
    'nor a never one' => '#[\NoDiscard] function n2(): never { throw new Exception("x"); }',
    'a void METHOD says method' => 'class C { #[\NoDiscard] public function m(): void {} }',
    'and so does a never one' => 'class C { #[\NoDiscard] public function m(): never { throw new Exception("x"); } }',
    'a constructor may not claim it at all' => 'class C { #[\NoDiscard] public function __construct() {} }',
    'an untyped function may' => '#[\NoDiscard] function w2() { return 1; } w2();',
    '(void) needs an expression' => '(void); echo "ok\n";',
    '(void) is not an operator' => '$y = (void) f();',
    'not in an argument either' => 'var_dump((void) 5);',
    'nor twice' => '(void) (void) f();',
    'nor after return' => 'function q() { return (void) f(); }',
    'nor in an arrow body' => '$g = fn() => (void) f();',
    'but it casts a whole statement' => '$x = 1; (void) $x = 5; echo $x, "\n";',
    'and a plain value' => '$z = 1; (void) $z; echo "ok\n";',
    'each for-clause element takes one' => 'for ((void) f(), $i = 0; $i < 1; $i++, (void) f()) {} echo "ok\n";',
    'and each one that lacks it warns' => 'for ($i = 0; $i < 1; $i++, f()) {} echo "ok\n";',
    'except the condition VALUE' => 'for ($i = 0; (void) f(); $i++) { break; }',
    'which may still precede it' => 'for ($i = 0; (void) f(), $i < 1; $i++) { break; } echo "ok\n";',
    '`void` is still an ordinary name' => 'const void = 5; echo void, "\n";',
    'and still a return type' => 'function r(): void {} r(); echo "ok\n";',
    'the attribute reflects' => '$a = (new ReflectionFunction("f"))->getAttributes()[0]; '
        . 'echo $a->getName(), "/", var_export($a->newInstance()->message, true), "\n";',
    'php marks the nine immutable mutators' => '$hit = []; foreach ((new ReflectionClass("DateTimeImmutable"))->getMethods() as $m) { '
        . 'foreach ($m->getAttributes() as $a) { if ($a->getName() === "NoDiscard") { $hit[] = $m->getName(); } } } '
        . 'sort($hit); echo implode(",", $hit), "\n";',
    'and dropping one of them warns' => '$d = new DateTimeImmutable("2020-01-01", new DateTimeZone("UTC")); $d->modify("+1 day");',
    'while the mutable twin does not' => '$d = new DateTime("2020-01-01", new DateTimeZone("UTC")); $d->modify("+1 day"); echo $d->format("Y-m-d"), "\n";',
];
/* Each case is its own COMPILE and its own diagnostic stream. */
foreach ($cases as $label => $code) {
    $f = tempnam(sys_get_temp_dir(), 'phlnd');
    file_put_contents($f, "<?php\n" . $pre . $code . "\n");
    $out = (string)shell_exec(escapeshellarg(PHP_BINARY) . ' ' . escapeshellarg($f) . ' 2>&1');
    /* a diagnostic names the script with symlinks resolved -- macOS's temp
     * dir sits behind /private -- so both spellings are scrubbed */
    $out = str_replace([realpath($f), $f], 'FILE', $out);
    $out = preg_replace('/^Stack trace:\n#0 \{main\}\n?/m', '', $out);
    $out = preg_replace('/\{closure:FILE:\d+\}/', '{closure}', $out);
    $out = preg_replace('/\{closure:C::go\(\):\d+\}/', '{closure}', $out);
    $out = trim(preg_replace('/ in FILE on line \d+/', '', $out));
    echo '### ', $label, "\n", ($out === '' ? '(silent)' : $out), "\n";
    unlink($f);
}
--EXPECT--
### a bare call statement
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
### the answer assigned
(silent)
### an intentional drop
(silent)
### the cast folds case
(silent)
### another cast consumes it
(silent)
### an operator consumes it
(silent)
### a condition consumes it
(silent)
### silence does not
(silent)
### a default does not
(silent)
### nor does a short-circuit
(silent)
### nor an argument
int(1)
### nor an array element
(silent)
### nor a ternary arm
(silent)
### nor echo
1
### every time round a loop
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
### never, if never called
no call
### through a variable name
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
### through a first-class callable
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
### not through array_map
(silent)
### but through call_user_func
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
### and call_user_func_array
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
### not when THAT answer is used
(silent)
### not when THAT one is cast
(silent)
### and not through a variable
(silent)
### a method
PHP Warning:  The return value of method C::m() should either be used or intentionally ignored by casting it as (void)
### a nullsafe one
PHP Warning:  The return value of method C::m() should either be used or intentionally ignored by casting it as (void)
### a static one
PHP Warning:  The return value of method C::s() should either be used or intentionally ignored by casting it as (void)
### named by the DECLARING class
PHP Warning:  The return value of method A::m() should either be used or intentionally ignored by casting it as (void)
### parent:: too
PHP Warning:  The return value of method A::m() should either be used or intentionally ignored by casting it as (void)
### a trait method takes the using class
PHP Warning:  The return value of method C::m() should either be used or intentionally ignored by casting it as (void)
### __invoke
PHP Warning:  The return value of method C::__invoke() should either be used or intentionally ignored by casting it as (void)
### an array callable
PHP Warning:  The return value of method C::m() should either be used or intentionally ignored by casting it as (void)
### a "C::m" string
PHP Warning:  The return value of method C::s() should either be used or intentionally ignored by casting it as (void)
### a closure
PHP Warning:  The return value of function {closure}() should either be used or intentionally ignored by casting it as (void)
### an arrow function
PHP Warning:  The return value of function {closure}() should either be used or intentionally ignored by casting it as (void)
### a closure in a class body is a METHOD
PHP Warning:  The return value of method C::{closure}() should either be used or intentionally ignored by casting it as (void)
### the message is appended
PHP Warning:  The return value of function w() should either be used or intentionally ignored by casting it as (void), because
### by name as well
PHP Warning:  The return value of function w() should either be used or intentionally ignored by casting it as (void), why
### null is no message
PHP Warning:  The return value of function w() should either be used or intentionally ignored by casting it as (void)
### and it is a constant EXPRESSION
PHP Warning:  The return value of function w() should either be used or intentionally ignored by casting it as (void), cm!
### a handler sees E_USER_WARNING
[512] The return value of function f() should either be used or intentionally ignored by casting it as (void)
### and it lands before the body
[body]
[warn]
### a void function may not claim it
PHP Fatal error:  A void function does not return a value, but #[\NoDiscard] requires a return value
### nor a never one
PHP Fatal error:  A never returning function does not return a value, but #[\NoDiscard] requires a return value
### a void METHOD says method
PHP Fatal error:  A void method does not return a value, but #[\NoDiscard] requires a return value
### and so does a never one
PHP Fatal error:  A never returning method does not return a value, but #[\NoDiscard] requires a return value
### a constructor may not claim it at all
PHP Fatal error:  Method C::__construct cannot be #[\NoDiscard]
### an untyped function may
PHP Warning:  The return value of function w2() should either be used or intentionally ignored by casting it as (void)
### (void) needs an expression
PHP Parse error:  syntax error, unexpected token ";"
### (void) is not an operator
PHP Parse error:  syntax error, unexpected token "(void)"
### not in an argument either
PHP Parse error:  syntax error, unexpected token "(void)"
### nor twice
PHP Parse error:  syntax error, unexpected token "(void)"
### nor after return
PHP Parse error:  syntax error, unexpected token "(void)", expecting ";"
### nor in an arrow body
PHP Parse error:  syntax error, unexpected token "(void)"
### but it casts a whole statement
5
### and a plain value
ok
### each for-clause element takes one
ok
### and each one that lacks it warns
PHP Warning:  The return value of function f() should either be used or intentionally ignored by casting it as (void)
ok
### except the condition VALUE
PHP Parse error:  syntax error, unexpected token ";", expecting ","
### which may still precede it
ok
### `void` is still an ordinary name
5
### and still a return type
ok
### the attribute reflects
NoDiscard/NULL
### php marks the nine immutable mutators
add,modify,setDate,setISODate,setMicrosecond,setTime,setTimestamp,setTimezone,sub
### and dropping one of them warns
PHP Warning:  The return value of method DateTimeImmutable::modify() should either be used or intentionally ignored by casting it as (void), as DateTimeImmutable::modify() does not modify the object itself
### while the mutable twin does not
2020-01-02
