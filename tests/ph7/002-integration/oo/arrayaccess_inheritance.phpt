--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ArrayAccess: subclass inherits implements relationship; subscript dispatches
--FILE--
<?php
abstract class AABase implements ArrayAccess {
    protected $data = [];
    public function offsetExists($k): bool { return isset($this->data[$k]); }
    #[\ReturnTypeWillChange]
    public function offsetGet($k) { return $this->data[$k] ?? null; }
    public function offsetSet($k, $v): void {
        if ($k === null) { $this->data[] = $v; } else { $this->data[$k] = $v; }
    }
    public function offsetUnset($k): void { unset($this->data[$k]); }
}
class AAChild extends AABase {}
$c = new AAChild();
echo "is AA: ", $c instanceof ArrayAccess ? "y" : "n", "\n";
$c["x"] = "hello";
echo "x=", $c["x"], "\n";
$r = isset($c["x"]) ? "y" : "n";
echo "isset(x): $r\n";
unset($c["x"]);
$r2 = isset($c["x"]) ? "y" : "n";
echo "after unset: $r2\n";
?>
--EXPECT--
is AA: y
x=hello
isset(x): y
after unset: n
--CLEAN--
<?php
