--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A generator's by-reference parameter aliases the caller's slot
--DESCRIPTION--
php binds a generator's arguments at the `g(...)` that BUILDS the Generator, and a
by-reference parameter is bound there to the CALLER's slot — so whenever the body
eventually runs, its write reaches the caller's variable. PHL COPIED every actual
into the generator's frame, so the write landed on the copy and the actual was
never touched, for a plain variable as much as for an element or a property. The
refusal half (a non-variable in a by-ref position) already shipped.

The frame outlives its caller, so the two cannot both own the slot: the caller's
teardown counts the body's name binding as a holder and leaves the value standing,
and the generator's own teardown releases it once nothing else holds it.

Only a SOURCE-LEVEL call binds this way. `Fiber::start()` and `call_user_func()`
pass by value (php warns there) and are deliberately unchanged.
--FILE--
<?php
function gbpBody(&$v) { $v = 'A'; yield 1; $v = 'B'; yield 2; }

echo "-- bound at the call, written when the body runs\n";
$a = 'a';
$it = gbpBody($a);
var_dump($a);
$it->current();
var_dump($a);
$it->next();
var_dump($a);

echo "-- a body that never runs writes nothing\n";
$b = 'b';
$unused = gbpBody($b);
var_dump($b);

echo "-- an element actual\n";
$arr = ['k' => 'o'];
$it2 = gbpBody($arr['k']);
$it2->current();
var_dump($arr['k']);

echo "-- a MISSING element is created and aliased\n";
$fresh = [];
$it3 = gbpBody($fresh['n']);
$it3->current();
var_dump($fresh);

echo "-- a property actual\n";
class GbpHolder { public $p = 'o'; }
$o = new GbpHolder;
$it4 = gbpBody($o->p);
$it4->current();
var_dump($o->p);

echo "-- a NAMED actual\n";
$c = 'c';
$it5 = gbpBody(v: $c);
$it5->current();
var_dump($c);

echo "-- a by-reference variadic tail\n";
function gbpTail(&...$xs) { $xs[0] = 'T'; yield 1; }
$d = 'd';
$it6 = gbpTail($d);
$it6->current();
var_dump($d);

echo "-- the caller's frame is gone before the body runs\n";
function gbpMake() {
    $z = 'z';
    return [gbpBody($z), function () use (&$z) { return $z; }];
}
[$it7, $read] = gbpMake();
$it7->current();
var_dump($read());

echo "-- the generator is dropped first\n";
function gbpDropFirst() {
    $z = 'z';
    $g = gbpBody($z);
    $g->current();
    unset($g);
    return $z;
}
var_dump(gbpDropFirst());

echo "-- two generators over one variable\n";
$s = 's';
$g1 = gbpBody($s);
$g2 = gbpBody($s);
$g1->current();
var_dump($s);
$g2->current();
var_dump($s);
unset($g1, $g2);
var_dump($s);

echo "-- a typed by-reference parameter\n";
function gbpTyped(int &$v) { $v = 9; yield 1; }
$t = 5;
$it8 = gbpTyped($t);
$it8->current();
var_dump($t);

echo "-- call_user_func() passes by VALUE\n";
$e = 'e';
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });
$it9 = call_user_func('gbpBody', $e);
restore_error_handler();
$it9->current();
var_dump($e);
?>
--EXPECT--
-- bound at the call, written when the body runs
string(1) "a"
string(1) "A"
string(1) "B"
-- a body that never runs writes nothing
string(1) "b"
-- an element actual
string(1) "A"
-- a MISSING element is created and aliased
array(1) {
  ["n"]=>
  &string(1) "A"
}
-- a property actual
string(1) "A"
-- a NAMED actual
string(1) "A"
-- a by-reference variadic tail
string(1) "T"
-- the caller's frame is gone before the body runs
string(1) "A"
-- the generator is dropped first
string(1) "A"
-- two generators over one variable
string(1) "A"
string(1) "A"
string(1) "A"
-- a typed by-reference parameter
int(9)
-- call_user_func() passes by VALUE
<gbpBody(): Argument #1 ($v) must be passed by reference, value given>
string(1) "e"
--CLEAN--
<?php
