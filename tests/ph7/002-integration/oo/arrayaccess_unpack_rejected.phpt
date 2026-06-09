--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Array unpacking [...$obj] is rejected on ArrayAccess-only object (must be Traversable)
--FILE--
<?php
class UnpackBag implements ArrayAccess {
    public function offsetExists($k): bool { return false; }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return null; }
    public function offsetSet($k, $v): void {}
    public function offsetUnset($k): void {}
}
$b = new UnpackBag();
try {
    $r = [...$b];
} catch (TypeError $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECTF--
caught: Only arrays and Traversables can be %s
--CLEAN--
<?php
