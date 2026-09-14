--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exception $trace/$previous are private, hidden from a subclass get_object_vars
--FILE--
<?php
class MyExc extends Exception {
    private $own = 'x';
    public function __serialize(): array { return get_object_vars($this); }
    public function keys() { $k = array_keys(get_object_vars($this)); sort($k); return $k; }
}
$e = new MyExc("m", 7);
var_dump($e->keys());
var_dump(in_array('trace', $e->keys(), true));
var_dump(in_array('previous', $e->keys(), true));
$s = $e->__serialize();
var_dump(count($s));
// getters still read the private fields (same-class scope)
var_dump(is_array($e->getTrace()));
var_dump($e->getPrevious());
?>
--EXPECT--
array(5) {
  [0]=>
  string(4) "code"
  [1]=>
  string(4) "file"
  [2]=>
  string(4) "line"
  [3]=>
  string(7) "message"
  [4]=>
  string(3) "own"
}
bool(false)
bool(false)
int(5)
bool(true)
NULL
--CLEAN--
<?php
