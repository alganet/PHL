--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: $obj[$key] = $value dispatches to offsetSet round-trip
--FILE--
<?php
class Bag implements ArrayAccess {
    public $data = [];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {
        echo "set($k,$v)\n";
        if ($k === null) { $this->data[] = $v; } else { $this->data[$k] = $v; }
    }
    public function offsetUnset($k): void {}
}
$b = new Bag();
$b["x"] = "X";
$b["y"] = "Y";
$b[10] = "ten";
echo $b["x"], "/", $b["y"], "/", $b[10], "\n";
?>
--EXPECT--
set(x,X)
set(y,Y)
set(10,ten)
X/Y/ten
--CLEAN--
<?php
