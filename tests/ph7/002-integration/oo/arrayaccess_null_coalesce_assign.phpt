--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: $obj[$k] ??= $v dispatches to offsetExists, offsetGet, and offsetSet per PHP semantics
--FILE--
<?php
class NCAssignBag implements ArrayAccess {
    public $data = ["set" => "old", "isnull" => null];
    public function offsetExists($k): bool { echo "exists($k)\n"; return array_key_exists($k, $this->data); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { echo "get($k)\n"; return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void { echo "set($k,$v)\n"; $this->data[$k] = $v; }
    public function offsetUnset($k): void {}
}
$b = new NCAssignBag();

echo "--- existing non-null key:\n";
$b["set"] ??= "fallback";
echo "result: ", $b["set"], "\n";

echo "--- existing null key:\n";
$b["isnull"] ??= "filled";
echo "result: ", $b["isnull"], "\n";

echo "--- missing key:\n";
$b["new"] ??= "added";
echo "result: ", $b["new"], "\n";
?>
--EXPECT--
--- existing non-null key:
exists(set)
get(set)
result: get(set)
old
--- existing null key:
exists(isnull)
get(isnull)
set(isnull,filled)
result: get(isnull)
filled
--- missing key:
exists(new)
set(new,added)
result: get(new)
added
--CLEAN--
<?php
