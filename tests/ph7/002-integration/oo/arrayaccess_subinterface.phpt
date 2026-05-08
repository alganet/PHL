--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: class implementing a sub-interface that extends ArrayAccess dispatches
--FILE--
<?php
interface MyAccess extends ArrayAccess {
    public function describe(): string;
}
class MyMap implements MyAccess {
    private $data = [];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {
        if ($k === null) { $this->data[] = $v; } else { $this->data[$k] = $v; }
    }
    public function offsetUnset($k): void { unset($this->data[$k]); }
    public function describe(): string { return "MyMap(" . count($this->data) . ")"; }
}
$m = new MyMap();
echo "is MyAccess: ", $m instanceof MyAccess ? "y" : "n", "\n";
echo "is ArrayAccess: ", $m instanceof ArrayAccess ? "y" : "n", "\n";
$m["k"] = 1;
$m["v"] = 2;
echo "k=", $m["k"], " v=", $m["v"], "\n";
echo $m->describe(), "\n";
?>
--EXPECT--
is MyAccess: y
is ArrayAccess: y
k=1 v=2
MyMap(2)
--CLEAN--
<?php
