--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A parenthesised expression is a valid left operand for -> and ?->
--FILE--
<?php
class AopeHolder {
    public $p = 'prop';
    public function m() { return 'method'; }
}
$o = new AopeHolder();

/* php's `( expr )` is dereferencable whatever it evaluates to, which is how the closure
 * idioms are written: the literal carries no operator, so a "must be a variable" rule
 * rejects valid php. */
echo "closure-bind:", (function () { return "lambda"; })->bindTo(null)(), "\n";
echo "arrow-bind:", (fn() => "arrow")->bindTo(null)(), "\n";
echo "closure-call:", (function () { return $this->p; })->call($o), "\n";
echo "match-method:", (match (1) { 1 => new AopeHolder() })->m(), "\n";
echo "match-property:", (match (1) { 1 => new AopeHolder() })->p, "\n";
echo "new:", (new AopeHolder())->m(), "\n";
echo "double-parens:", ((new AopeHolder()))->m(), "\n";
echo "variable:", ($o)->m(), "\n";
echo "nullsafe:", var_export((null)?->x, true), "\n";
echo "ternary:", (true ? $o : null)->m(), "\n";
echo "clone:", (clone $o)->m(), "\n";
?>
--EXPECT--
closure-bind:lambda
arrow-bind:arrow
closure-call:prop
match-method:method
match-property:prop
new:method
double-parens:method
variable:method
nullsafe:NULL
ternary:method
clone:method
--CLEAN--
<?php
