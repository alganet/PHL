--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: an exception thrown while evaluating the ??= RHS disarms the coalesce slot so a later unrelated ??= is unaffected
--FILE--
<?php
class NCThrowBag implements ArrayAccess {
    public $data = [];
    public function offsetExists($k): bool { return array_key_exists($k, $this->data); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void { echo "offsetSet($k, $v)\n"; $this->data[$k] = $v; }
    public function offsetUnset($k): void {}
}
function boom() { throw new Exception("boom"); }

$b = new NCThrowBag();

// LOAD_IDX iP2=3 arms the coalesce slot for the missing key "stale", then the
// RHS throws before NULLC_STORE runs. The throw must disarm the slot.
try {
    $b["stale"] ??= boom();
} catch (Exception $e) {
    echo "caught: ", $e->getMessage(), "\n";
}

// An unrelated ??= on a plain variable: NULLC_STORE must write into $x, NOT
// dispatch offsetSet() on the stale (object, key).
$x = null;
$x ??= "plain";
echo "x = ", $x, "\n";
echo "bag has 'stale': ", (isset($b["stale"]) ? "yes" : "no"), "\n";
?>
--EXPECT--
caught: boom
x = plain
bag has 'stale': no
--CLEAN--
<?php
unset($b);
