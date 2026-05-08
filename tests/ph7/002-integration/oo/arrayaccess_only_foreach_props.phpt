--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
foreach over an object that only implements ArrayAccess (no Iterator) iterates public properties — does NOT call offsetGet
--FILE--
<?php
class OnlyAA implements ArrayAccess {
    public $alpha = 1;
    public $beta = 2;
    public function offsetExists($k): bool { return false; }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { echo "offsetGet should NOT be called\n"; return null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$o = new OnlyAA();
foreach ($o as $k => $v) {
    echo "$k=$v\n";
}
?>
--EXPECT--
alpha=1
beta=2
--CLEAN--
<?php
