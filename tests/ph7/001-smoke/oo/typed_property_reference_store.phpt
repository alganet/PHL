--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A reference to a typed property still carries the property's type
--DESCRIPTION--
`$r = &$o->n` aliases the property's slot, so a write through `$r` is a write
to the property — and php enforces the declared type there exactly as it does
on `$o->n = v`, with a sentence of its own naming the property that HOLDS the
reference. This engine enforced only the write spelled as a property, so a
string landed in an `int` property and an array in a `string` one, silently,
and every reader after it saw a value its declaration says cannot be there. A
by-ref PARAMETER bound to the property is the same slot and the same rule. The
coercions are unchanged: a numeric string still becomes the int, an int still
becomes the string.
--FILE--
<?php
class TrefBox {
    public int $n = 1;
    public array $a = [];
    public ?TrefBox $o = null;
    public string $s = 'x';
    public $untyped = 1;
}
function trefTry($what, $fn) {
    try { $fn(); echo "$what => ok\n"; }
    catch (Throwable $e) { echo "$what => ", get_class($e), ": ", $e->getMessage(), "\n"; }
}
$trefB = new TrefBox;
/* A reference to a typed property carries the property's type: php enforces
 * the store and names the property HOLDING the reference. */
$trefRn = &$trefB->n;
trefTry('$r = "no"   (int $n)', function () use (&$trefRn) { $trefRn = 'no'; });
trefTry('$r = "7"    (int $n)', function () use (&$trefRn) { $trefRn = '7'; });
echo '  n is now: ', var_export($trefB->n, true), "\n";
$trefRa = &$trefB->a;
trefTry('$r = "no"   (array $a)', function () use (&$trefRa) { $trefRa = 'no'; });
trefTry('$r = [1]    (array $a)', function () use (&$trefRa) { $trefRa = [1]; });
$trefRo = &$trefB->o;
trefTry('$r = 5      (?TrefBox $o)', function () use (&$trefRo) { $trefRo = 5; });
trefTry('$r = null   (?TrefBox $o)', function () use (&$trefRo) { $trefRo = null; });
$trefRs = &$trefB->s;
trefTry('$r = [1]    (string $s)', function () use (&$trefRs) { $trefRs = [1]; });
trefTry('$r = 5      (string $s)', function () use (&$trefRs) { $trefRs = 5; });
echo '  s is now: ', var_export($trefB->s, true), "\n";
/* The UNTYPED property beside them takes anything, as it always did. */
$trefRu = &$trefB->untyped;
trefTry('$r = [1]    (untyped)', function () use (&$trefRu) { $trefRu = [1]; });
/* A by-ref PARAMETER bound to a typed property is the same slot. */
function trefWrite(&$v, $x) { $v = $x; }
$trefB2 = new TrefBox;
trefTry('f(&$o->n) writes "no"', fn() => trefWrite($trefB2->n, 'no'));
trefTry('f(&$o->n) writes 42',   fn() => trefWrite($trefB2->n, 42));
echo '  n is now: ', var_export($trefB2->n, true), "\n";
/* And a write through the PROPERTY still says "property". */
trefTry('$o->n = "no"', function () use ($trefB2) { $trefB2->n = 'no'; });
--EXPECT--
$r = "no"   (int $n) => TypeError: Cannot assign string to reference held by property TrefBox::$n of type int
$r = "7"    (int $n) => ok
  n is now: 7
$r = "no"   (array $a) => TypeError: Cannot assign string to reference held by property TrefBox::$a of type array
$r = [1]    (array $a) => ok
$r = 5      (?TrefBox $o) => TypeError: Cannot assign int to reference held by property TrefBox::$o of type ?TrefBox
$r = null   (?TrefBox $o) => ok
$r = [1]    (string $s) => TypeError: Cannot assign array to reference held by property TrefBox::$s of type string
$r = 5      (string $s) => ok
  s is now: '5'
$r = [1]    (untyped) => ok
f(&$o->n) writes "no" => TypeError: Cannot assign string to reference held by property TrefBox::$n of type int
f(&$o->n) writes 42 => ok
  n is now: 42
$o->n = "no" => TypeError: Cannot assign string to property TrefBox::$n of type int
