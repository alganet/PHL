--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: get_class_methods() lists only what the CALLING scope could reach
--DESCRIPTION--
php filters the method list by the executing scope, exactly as it filters
get_class_vars(): public always, protected from within the hierarchy, private
only from the class that declares it. PHL returned the whole table, handing
global-scope code every private and protected name a class holds.
--FILE--
<?php
class GcmA {
    public function pubA() {}
    protected function protA() {}
    private function privA() {}
    public static function statA() {}
    public function inA() { return get_class_methods('GcmA'); }
}
class GcmB extends GcmA {
    public function pubB() {}
    private function privB() {}
    public function inB() { return [get_class_methods('GcmB'), get_class_methods('GcmA')]; }
}
class GcmOutside {
    public function look() { return [get_class_methods('GcmA'), get_class_methods('GcmB')]; }
}

/* Global scope: the public surface only. */
var_dump(get_class_methods('GcmA'));
var_dump(get_class_methods('GcmB'));
/* An object argument answers the same as its class name. */
var_dump(get_class_methods(new GcmB) === get_class_methods('GcmB'));

/* Inside the declaring class: everything it declares. */
var_dump((new GcmA)->inA());
/* Inside a SUBCLASS: its own private, the inherited protected — but not the
   base's private, which the child scope cannot reach. */
var_dump((new GcmB)->inB());
/* An unrelated class is no better off than global scope. */
var_dump((new GcmOutside)->look());
?>
--EXPECT--
array(3) {
  [0]=>
  string(4) "pubA"
  [1]=>
  string(5) "statA"
  [2]=>
  string(3) "inA"
}
array(5) {
  [0]=>
  string(4) "pubB"
  [1]=>
  string(3) "inB"
  [2]=>
  string(4) "pubA"
  [3]=>
  string(5) "statA"
  [4]=>
  string(3) "inA"
}
bool(true)
array(5) {
  [0]=>
  string(4) "pubA"
  [1]=>
  string(5) "protA"
  [2]=>
  string(5) "privA"
  [3]=>
  string(5) "statA"
  [4]=>
  string(3) "inA"
}
array(2) {
  [0]=>
  array(7) {
    [0]=>
    string(4) "pubB"
    [1]=>
    string(5) "privB"
    [2]=>
    string(3) "inB"
    [3]=>
    string(4) "pubA"
    [4]=>
    string(5) "protA"
    [5]=>
    string(5) "statA"
    [6]=>
    string(3) "inA"
  }
  [1]=>
  array(4) {
    [0]=>
    string(4) "pubA"
    [1]=>
    string(5) "protA"
    [2]=>
    string(5) "statA"
    [3]=>
    string(3) "inA"
  }
}
array(2) {
  [0]=>
  array(3) {
    [0]=>
    string(4) "pubA"
    [1]=>
    string(5) "statA"
    [2]=>
    string(3) "inA"
  }
  [1]=>
  array(5) {
    [0]=>
    string(4) "pubB"
    [1]=>
    string(3) "inB"
    [2]=>
    string(4) "pubA"
    [3]=>
    string(5) "statA"
    [4]=>
    string(3) "inA"
  }
}
--CLEAN--
<?php
