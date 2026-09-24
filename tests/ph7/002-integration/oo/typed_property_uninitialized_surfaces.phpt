--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An uninitialized typed property is absent from every value surface, and var_dump marks it
--FILE--
<?php
interface TpuA {}
interface TpuB {}

class TpuHolder {
    public int $int;
    public ?string $nullable;
    public array|int $union;
    public self $scoped;
    public TpuA&TpuB $intersect;
    public iterable $iterable;
    public readonly float $ro;
    public int $written = 5;
    protected string $prot;
    private float $priv;
    public static int $stat = 1;
}

$o = new TpuHolder;
var_dump($o);
print_r($o);
var_export($o);
echo "\n";
var_dump(get_object_vars($o));
var_dump(array_keys((array)$o));
var_dump(array_keys(get_mangled_object_vars($o)));
echo json_encode($o), "\n";
echo serialize($o), "\n";
foreach ($o as $k => $v) { echo "iter: $k\n"; }

$c = clone $o;
var_dump(count((array)$c));
try { $c->int; } catch (Throwable $t) { echo get_class($t), ': ', $t->getMessage(), "\n"; }

$o->int = 7;
var_dump(array_keys((array)$o));
?>
--EXPECT--
object(TpuHolder)#1 (1) {
  ["int"]=>
  uninitialized(int)
  ["nullable"]=>
  uninitialized(?string)
  ["union"]=>
  uninitialized(array|int)
  ["scoped"]=>
  uninitialized(TpuHolder)
  ["intersect"]=>
  uninitialized(TpuA&TpuB)
  ["iterable"]=>
  uninitialized(Traversable|array)
  ["ro"]=>
  uninitialized(float)
  ["written"]=>
  int(5)
  ["prot":protected]=>
  uninitialized(string)
  ["priv":"TpuHolder":private]=>
  uninitialized(float)
}
TpuHolder Object
(
    [written] => 5
)
\TpuHolder::__set_state(array(
   'written' => 5,
))
array(1) {
  ["written"]=>
  int(5)
}
array(1) {
  [0]=>
  string(7) "written"
}
array(1) {
  [0]=>
  string(7) "written"
}
{"written":5}
O:9:"TpuHolder":1:{s:7:"written";i:5;}
iter: written
int(1)
Error: Typed property TpuHolder::$int must not be accessed before initialization
array(2) {
  [0]=>
  string(3) "int"
  [1]=>
  string(7) "written"
}
--CLEAN--
<?php
