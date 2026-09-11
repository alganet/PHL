--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionMethod::getDeclaringClass() resolves inherited methods via the subclass
--FILE--
<?php
class MdciBase {
    public function pub() {}
    protected function prot() {}
}
class MdciMid extends MdciBase {
    public function midOwn() {}
}
class MdciLeaf extends MdciMid {
    public function leafOwn() {}
}
$rc = new ReflectionClass('MdciLeaf');
$out = [];
foreach ($rc->getMethods() as $m) {
    $out[$m->getName()] = $m->getDeclaringClass()->getName();
}
ksort($out);
foreach ($out as $name => $decl) {
    echo "$name -> $decl\n";
}
// Also resolve by (subclass, inherited-name) directly.
echo (new ReflectionMethod('MdciLeaf', 'pub'))->getDeclaringClass()->getName(), "\n";
echo (new ReflectionMethod('MdciLeaf', 'prot'))->getDeclaringClass()->getName(), "\n";
?>
--EXPECT--
leafOwn -> MdciLeaf
midOwn -> MdciMid
prot -> MdciBase
pub -> MdciBase
MdciBase
MdciBase
--CLEAN--
<?php
