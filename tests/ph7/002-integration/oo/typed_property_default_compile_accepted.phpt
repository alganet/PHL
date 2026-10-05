--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: folded defaults php accepts at compile time, and the ones it leaves to the instantiation
--FILE--
<?php
const TpdcaStr = "a";
class TpdcaHolder {
    public int $div = 4 / 2;
    public float $wide = 1 + 2;
    public int|float $real = 1.0;
    public string $cls = TpdcaHolder::class;
    public ?string $none = null;
    public mixed $any = null;
    public bool $cmp = 1 < 2;
    public array $list = [1, [2]];
    public iterable $it = [];
    public int $shift = 1 << 3;
}
// A constant NAME is not folded: the wrong type surfaces only at `new`.
class TpdcaLate { public int $p = TpdcaStr; }
var_dump(get_object_vars(new TpdcaHolder));
try { new TpdcaLate; } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
--EXPECT--
array(10) {
  ["div"]=>
  int(2)
  ["wide"]=>
  float(3)
  ["real"]=>
  float(1)
  ["cls"]=>
  string(11) "TpdcaHolder"
  ["none"]=>
  NULL
  ["any"]=>
  NULL
  ["cmp"]=>
  bool(true)
  ["list"]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    array(1) {
      [0]=>
      int(2)
    }
  }
  ["it"]=>
  array(0) {
  }
  ["shift"]=>
  int(8)
}
Cannot assign string to property TpdcaLate::$p of type int
--CLEAN--
<?php
