--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: $obj instanceof ArrayAccess holds, multiple methods can interleave
--FILE--
<?php
class Bag implements ArrayAccess {
    public $data = [];
    public function offsetExists($k): bool { return array_key_exists($k, $this->data); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {
        if ($k === null) { $this->data[] = $v; } else { $this->data[$k] = $v; }
    }
    public function offsetUnset($k): void { unset($this->data[$k]); }
}
$b = new Bag();
echo "is_aa: ", $b instanceof ArrayAccess ? "y" : "n", "\n";
$b["k1"] = 100;
$b["k2"] = 200;
$b["k1"] = $b["k1"] + $b["k2"];
echo "k1=", $b["k1"], "\n";
echo "k2=", $b["k2"], "\n";
echo "exists k1: ", isset($b["k1"]) ? "y" : "n", "\n";
unset($b["k2"]);
echo "exists k2 after unset: ", isset($b["k2"]) ? "y" : "n", "\n";
echo "exists k1 after unset k2: ", isset($b["k1"]) ? "y" : "n", "\n";
?>
--EXPECT--
is_aa: y
k1=300
k2=200
exists k1: y
exists k2 after unset: n
exists k1 after unset k2: y
--CLEAN--
<?php
