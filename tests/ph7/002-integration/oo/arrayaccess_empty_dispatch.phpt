--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: empty() short-circuits on missing keys via offsetExists; calls offsetGet only when present
--FILE--
<?php
class EmptyBag implements ArrayAccess {
    public $data = ["one" => 1, "zero" => 0, "estr" => "", "nested" => [1,2]];
    public function offsetExists($k): bool { echo "exists($k)\n"; return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { echo "get($k)\n"; return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$b = new EmptyBag();
$r = empty($b["one"]) ? "y" : "n";
echo "empty(one): $r\n";
$r = empty($b["missing"]) ? "y" : "n";
echo "empty(missing): $r\n";
$r = empty($b["zero"]) ? "y" : "n";
echo "empty(zero): $r\n";
$r = empty($b["estr"]) ? "y" : "n";
echo "empty(estr): $r\n";
$r = empty($b["nested"]) ? "y" : "n";
echo "empty(nested): $r\n";
?>
--EXPECT--
exists(one)
get(one)
empty(one): n
exists(missing)
empty(missing): y
exists(zero)
get(zero)
empty(zero): y
exists(estr)
get(estr)
empty(estr): y
exists(nested)
get(nested)
empty(nested): n
--CLEAN--
<?php
