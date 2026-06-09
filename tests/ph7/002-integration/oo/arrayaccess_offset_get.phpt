--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: $obj[$key] dispatches to offsetGet with the key
--FILE--
<?php
class Bag implements ArrayAccess {
    public $data = ["alpha" => "A", "beta" => "B", 7 => "seven"];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { echo "get($k)\n"; return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$b = new Bag();
echo $b["alpha"], "\n";
echo $b["beta"], "\n";
echo $b[7], "\n";
?>
--EXPECT--
get(alpha)
A
get(beta)
B
get(7)
seven
--CLEAN--
<?php
