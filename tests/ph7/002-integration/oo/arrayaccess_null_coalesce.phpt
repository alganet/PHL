--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Null coalescing operator on ArrayAccess subscripts uses offsetGet result
--FILE--
<?php
class NCBag implements ArrayAccess {
    public $data = ["a" => "A", "n" => null];
    public function offsetExists($k): bool { return array_key_exists($k, $this->data); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void { $this->data[$k] = $v; }
    public function offsetUnset($k): void { unset($this->data[$k]); }
}
$b = new NCBag();
echo $b["a"] ?? "fallback", "\n";
echo $b["missing"] ?? "fallback", "\n";
echo $b["n"] ?? "fallback", "\n";
?>
--EXPECT--
A
fallback
fallback
--CLEAN--
<?php
