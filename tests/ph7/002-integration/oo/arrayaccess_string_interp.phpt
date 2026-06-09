--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: string interpolation $b[key] and {$b['key']} dispatches to offsetGet
--FILE--
<?php
class InterpBag implements ArrayAccess {
    public $data = ["name" => "alice", "age" => 30];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$b = new InterpBag();
echo "simple: $b[name]\n";
echo "braced: {$b['age']}\n";
echo "concat: " . $b["name"] . "/" . $b["age"] . "\n";
?>
--EXPECT--
simple: alice
braced: 30
concat: alice/30
--CLEAN--
<?php
