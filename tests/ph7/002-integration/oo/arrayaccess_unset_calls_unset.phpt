--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: unset($obj[$key]) dispatches to offsetUnset
--FILE--
<?php
class Bag implements ArrayAccess {
    public $data = ["a" => 1, "b" => 2];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {
        echo "unset($k)\n";
        unset($this->data[$k]);
    }
}
$b = new Bag();
unset($b["a"]);
$ax = isset($b["a"]) ? "y" : "n";
$bx = isset($b["b"]) ? "y" : "n";
echo "after unset(a): a=$ax b=$bx\n";
unset($b["b"]);
$ay = isset($b["a"]) ? "y" : "n";
$by = isset($b["b"]) ? "y" : "n";
echo "after unset(b): a=$ay b=$by\n";
?>
--EXPECT--
unset(a)
after unset(a): a=n b=y
unset(b)
after unset(b): a=n b=n
--CLEAN--
<?php
