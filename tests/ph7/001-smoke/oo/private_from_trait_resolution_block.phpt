--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A trait's private property composed through a CONFLICT-RESOLUTION block still owns its own slot below
--DESCRIPTION--
A trait's private property belongs to the class that composes it, so a subclass
that declares the same name has a second slot and the composing class's code keeps
reaching its own (php's mangled storage name). The engine files trait properties
from two different places -- the plain `use T;` path and the `use T1, T2 { ...
insteadof ... }` one -- and only the first of them had a test. The screen that
decides, on every property access, whether a mangled slot can be involved at all is
built from what those two paths record, so a class composed through the resolution
block is exactly the shape that would read the WRONG slot with nothing to announce
it.
--FILE--
<?php
trait ShadowResA {
    private $srp = 'A-private';
    public function readOwn() { return $this->srp; }
    public function shared()  { return 'from-A'; }
}
trait ShadowResB {
    public function shared()  { return 'from-B'; }
}
class ShadowResBase {
    use ShadowResA, ShadowResB { ShadowResA::shared insteadof ShadowResB; }
}
class ShadowResChild extends ShadowResBase {
    public $srp = 'child-public';
    public function childRead() { return $this->srp; }
}
$shadowRes = new ShadowResChild();
echo "base scope: ", $shadowRes->readOwn(), "\n";
echo "child scope: ", $shadowRes->childRead(), "\n";
echo "outside: ", $shadowRes->srp, "\n";
echo "shared: ", $shadowRes->shared(), "\n";
var_dump($shadowRes);
echo json_encode(get_object_vars($shadowRes)), "\n";
echo "own class: ", (new ShadowResBase())->readOwn(), "\n";
?>
--EXPECTF--
base scope: A-private
child scope: child-public
outside: child-public
shared: from-A
object(ShadowResChild)#%d (2) {
  ["srp":"ShadowResBase":private]=>
  string(9) "A-private"
  ["srp"]=>
  string(12) "child-public"
}
{"srp":"child-public"}
own class: A-private
