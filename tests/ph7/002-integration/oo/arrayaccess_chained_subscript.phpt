--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: chained subscript $obj[k][i] reads via offsetGet then array/string indexing
--FILE--
<?php
class ChainBag implements ArrayAccess {
    public $data = [
        "row"  => [10, 20, 30],
        "name" => "alice",
        "map"  => ["x" => 1, "y" => 2],
    ];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$b = new ChainBag();
echo $b["row"][0], "\n";
echo $b["row"][2], "\n";
echo $b["name"][0], "\n";
echo $b["map"]["x"], "+", $b["map"]["y"], "=", ($b["map"]["x"] + $b["map"]["y"]), "\n";
?>
--EXPECT--
10
30
a
1+2=3
--CLEAN--
<?php
