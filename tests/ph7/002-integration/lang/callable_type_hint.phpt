--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
the `callable` type is is_callable() — parameters, returns, variadics and unions
--FILE--
<?php
class Oth {}
class Inv {
    public function __invoke() {}
    public function m() {}
    public static function s() {}
    private function priv() {}
}

function takesCallable(callable $c) { echo "accepted\n"; }

$cases = [
    'int'               => 5,
    'float'             => 1.5,
    'bool'              => true,
    'null'              => null,
    'array-empty'       => [],
    'string-function'   => 'strlen',
    'string-missing'    => 'nosuchfunction123',
    'string-empty'      => '',
    'string-static'     => 'Inv::s',
    'string-nonstatic'  => 'Inv::m',
    'array-object'      => [new Inv, 'm'],
    'array-classname'   => ['Inv', 's'],
    'array-nonstatic'   => ['Inv', 'm'],
    'array-not-a-pair'  => [1, 2],
    'array-three'       => [new Inv, 'm', 'x'],
    'array-private'     => [new Inv, 'priv'],
    'closure'           => function () {},
    'invokable'         => new Inv,
    'plain-object'      => new Oth,
];
foreach ($cases as $label => $value) {
    echo str_pad($label, 20);
    try { takesCallable($value); }
    catch (TypeError $e) { echo 'REJECT: ', strstr($e->getMessage(), ', called in', true), "\n"; }
}

// The same predicate has to answer for is_callable(), or the type and the
// builtin could disagree about the very same value.
echo "== agrees with is_callable() ==\n";
foreach ($cases as $label => $value) {
    $typeOk = true;
    try { (function (callable $c) {})($value); } catch (TypeError $e) { $typeOk = false; }
    if ($typeOk !== is_callable($value)) { echo "DISAGREE: $label\n"; }
}
echo "checked\n";

// Returns, variadic elements and union members carry the same check.
echo "== other positions ==\n";
function retBad(): callable { return new Oth; }
function retOk(): callable { return 'strlen'; }
function retNullable(): ?callable { return null; }
function retUnion(): callable|int { return new Oth; }
function variadic(callable ...$c) { echo "variadic ok\n"; }
function union(callable|int $x) { echo "union ok\n"; }

// A call-site TypeError carries a `, called in <file> on line <n>` tail; a
// return-value one does not. Drop it either way so the file path stays out.
$show = function (callable $fn) {
    try { $fn(); }
    catch (TypeError $e) {
        $m = $e->getMessage();
        $cut = strpos($m, ', called in');
        echo $cut === false ? $m : substr($m, 0, $cut), "\n";
    }
};
$show(fn() => retBad());
$show(fn() => var_dump(retOk()));
$show(fn() => var_dump(retNullable()));
$show(fn() => retUnion());
$show(fn() => variadic('strlen', new Inv));
$show(fn() => variadic('strlen', new Oth));
$show(fn() => union(3));
$show(fn() => union(new Oth));
?>
--EXPECT--
int                 REJECT: takesCallable(): Argument #1 ($c) must be of type callable, int given
float               REJECT: takesCallable(): Argument #1 ($c) must be of type callable, float given
bool                REJECT: takesCallable(): Argument #1 ($c) must be of type callable, true given
null                REJECT: takesCallable(): Argument #1 ($c) must be of type callable, null given
array-empty         REJECT: takesCallable(): Argument #1 ($c) must be of type callable, array given
string-function     accepted
string-missing      REJECT: takesCallable(): Argument #1 ($c) must be of type callable, string given
string-empty        REJECT: takesCallable(): Argument #1 ($c) must be of type callable, string given
string-static       accepted
string-nonstatic    REJECT: takesCallable(): Argument #1 ($c) must be of type callable, string given
array-object        accepted
array-classname     accepted
array-nonstatic     REJECT: takesCallable(): Argument #1 ($c) must be of type callable, array given
array-not-a-pair    REJECT: takesCallable(): Argument #1 ($c) must be of type callable, array given
array-three         REJECT: takesCallable(): Argument #1 ($c) must be of type callable, array given
array-private       REJECT: takesCallable(): Argument #1 ($c) must be of type callable, array given
closure             accepted
invokable           accepted
plain-object        REJECT: takesCallable(): Argument #1 ($c) must be of type callable, Oth given
== agrees with is_callable() ==
checked
== other positions ==
retBad(): Return value must be of type callable, Oth returned
string(6) "strlen"
NULL
retUnion(): Return value must be of type callable|int, Oth returned
variadic ok
variadic(): Argument #2 must be of type callable, Oth given
union ok
union(): Argument #1 ($x) must be of type callable|int, Oth given
--CLEAN--
<?php
