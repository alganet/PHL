--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: $obj[] = value passes null key to offsetSet
--FILE--
<?php
class Bag implements ArrayAccess {
    public $data = [];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {
        echo "set(", ($k === null ? "NULL" : $k), ",$v)\n";
        if ($k === null) { $this->data[] = $v; } else { $this->data[$k] = $v; }
    }
    public function offsetUnset($k): void {}
}
$b = new Bag();
$b[] = "first";
$b[] = "second";
$b["named"] = "n";
$b[] = "third";
echo implode(",", $b->data), "\n";
?>
--EXPECT--
set(NULL,first)
set(NULL,second)
set(named,n)
set(NULL,third)
first,second,n,third
--CLEAN--
<?php
