--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset() with multiple arguments mixing array, scalar, and ArrayAccess subscript
--FILE--
<?php
class IsetBag implements ArrayAccess {
    public $data = ["k" => 1];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$b = new IsetBag();
$arr = ["x" => 1];
$scalar = "hello";
$null = null;
$r = isset($scalar, $arr["x"], $b["k"]) ? "y" : "n";
echo "all-set: $r\n";
$r = isset($scalar, $arr["miss"], $b["k"]) ? "y" : "n";
echo "miss-arr: $r\n";
$r = isset($scalar, $arr["x"], $b["miss"]) ? "y" : "n";
echo "miss-obj: $r\n";
$r = isset($null, $b["k"]) ? "y" : "n";
echo "null-var: $r\n";
?>
--EXPECT--
all-set: y
miss-arr: n
miss-obj: n
null-var: n
--CLEAN--
<?php
