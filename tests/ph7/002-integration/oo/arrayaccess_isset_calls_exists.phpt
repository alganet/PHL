--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: isset($obj[$key]) calls offsetExists, never offsetGet
--FILE--
<?php
class Bag implements ArrayAccess {
    public $data = ["present" => 1];
    public function offsetExists($k): bool {
        echo "exists($k)\n";
        return isset($this->data[$k]);
    }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) {
        echo "get($k)\n";
        return $this->data[$k] ?? null;
    }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$b = new Bag();
$r1 = isset($b["present"]) ? "yes" : "no";
echo "present: $r1\n";
$r2 = isset($b["missing"]) ? "yes" : "no";
echo "missing: $r2\n";
?>
--EXPECT--
exists(present)
present: yes
exists(missing)
missing: no
--CLEAN--
<?php
