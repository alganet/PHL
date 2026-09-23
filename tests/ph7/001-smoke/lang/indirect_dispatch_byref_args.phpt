--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An array-callable value and an __invoke object see the callee's by-ref formals
--DESCRIPTION--
Both indirect dispatch paths reach their target through a shared helper that hid the
callee from OP_CALL, so neither could ask what its parameters actually are. They
materialized every deferred plain-variable argument BY REFERENCE and every deferred
element/property BY VALUE, which is wrong in both directions: a genuine by-reference
out-param into a missing element warned `Undefined array key` and handed the callee
a NULL to write into, while a by-VALUE parameter swallowed php's `Undefined
variable` and CREATED the caller's variable.

The target is knowable at the dispatch — the pair resolves to a class and a method,
an object to its `__invoke` — so the arguments are resolved against it, with a
native method's signature mask and a PHP one's formals, exactly as a direct call
does. A name routed through `__call`/`__callStatic` resolves to nothing and binds by
value, which is php's answer too (it packs those into an array).
--FILE--
<?php
class IdaTarget {
    public static function byRef(&$x) { $x = 'S'; }
    public static function byVal($x) { var_dump($x); }
    public function meth(&$x) { $x = 'M'; }
}
class IdaInvokeRef { public function __invoke(&$x) { $x = 'V'; } }
class IdaInvokeVal { public function __invoke($x) { var_dump($x); } }
class IdaMagic {
    public function __call($n, $a) { var_dump($n, $a); }
    public static function __callStatic($n, $a) { var_dump($n, $a); }
}

$ref = ['IdaTarget', 'byRef'];
$val = ['IdaTarget', 'byVal'];

echo "-- by-ref parameter CREATES a missing element\n";
$a = [];
$ref($a['new']);
var_dump($a);

echo "-- by-ref parameter writes an existing element\n";
$b = ['k' => 'o'];
$ref($b['k']);
var_dump($b);

echo "-- by-ref parameter writes a property\n";
class IdaHolder { public $p = 'o'; }
$h = new IdaHolder;
$ref($h->p);
var_dump($h->p);

echo "-- an object-method pair\n";
$obj = new IdaTarget;
$pair = [$obj, 'meth'];
$c = [];
$pair($c['new']);
var_dump($c);

echo "-- __invoke, by-ref parameter\n";
$inv = new IdaInvokeRef;
$d = [];
$inv($d['new']);
var_dump($d);

echo "-- __invoke, by-VALUE parameter, undefined variable\n";
$invVal = new IdaInvokeVal;
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });
$invVal($idaUndef1);
restore_error_handler();
var_dump(array_key_exists('idaUndef1', get_defined_vars()));

echo "-- pair, by-VALUE parameter, undefined variable\n";
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });
$val($idaUndef2);
restore_error_handler();
var_dump(array_key_exists('idaUndef2', get_defined_vars()));

echo "-- pair, by-VALUE parameter, missing element\n";
$e = [];
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });
$val($e['new']);
restore_error_handler();
var_dump($e);

echo "-- a __call route packs by VALUE\n";
$magic = [new IdaMagic, 'nope'];
set_error_handler(function ($n, $m) { echo '<', $m, '>', "\n"; return true; });
$magic($idaUndef3);
restore_error_handler();
var_dump(array_key_exists('idaUndef3', get_defined_vars()));

echo "-- a Closure over a by-ref method pair\n";
$cl = Closure::fromCallable([new IdaTarget, 'meth']);
$f = [];
$cl($f['new']);
var_dump($f);

echo "-- a NATIVE method through a pair\n";
$ao = new ArrayObject([1, 2, 3]);
$nat = [$ao, 'offsetSet'];
$nat(0, 'X');
var_dump($ao[0]);
?>
--EXPECT--
-- by-ref parameter CREATES a missing element
array(1) {
  ["new"]=>
  string(1) "S"
}
-- by-ref parameter writes an existing element
array(1) {
  ["k"]=>
  string(1) "S"
}
-- by-ref parameter writes a property
string(1) "S"
-- an object-method pair
array(1) {
  ["new"]=>
  string(1) "M"
}
-- __invoke, by-ref parameter
array(1) {
  ["new"]=>
  string(1) "V"
}
-- __invoke, by-VALUE parameter, undefined variable
<Undefined variable $idaUndef1>
NULL
bool(false)
-- pair, by-VALUE parameter, undefined variable
<Undefined variable $idaUndef2>
NULL
bool(false)
-- pair, by-VALUE parameter, missing element
<Undefined array key "new">
NULL
array(0) {
}
-- a __call route packs by VALUE
<Undefined variable $idaUndef3>
string(4) "nope"
array(1) {
  [0]=>
  NULL
}
bool(false)
-- a Closure over a by-ref method pair
array(1) {
  ["new"]=>
  string(1) "M"
}
-- a NATIVE method through a pair
string(1) "X"
--CLEAN--
<?php
