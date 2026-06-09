--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: subscript read/write dispatches to offsetGet/offsetSet
--FILE--
<?php
class IfaceArrayAccessBag implements ArrayAccess {
    public $data = [];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {
        if ($k === null) { $this->data[] = $v; } else { $this->data[$k] = $v; }
    }
    public function offsetUnset($k): void { unset($this->data[$k]); }
}
$b = new IfaceArrayAccessBag();
$b["a"] = 1;
$b["b"] = 2;
echo $b["a"], "/", $b["b"], "\n";
?>
--EXPECT--
1/2
--CLEAN--
<?php
