--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A constructor-promoted property with a union/intersection type is enforced on a later store
--FILE--
<?php
interface PpA {}
interface PpB {}
class PpAB implements PpA, PpB {}

class PpUnion {
    public function __construct(public int|string $x) {}
}
$u = new PpUnion(5);
echo $u->x, "\n";
$u->x = "ok";
echo $u->x, "\n";
try { $u->x = [1, 2, 3]; } catch (\TypeError $e) { echo "union store TE\n"; }

class PpIsect {
    public function __construct(public PpA&PpB $y) {}
}
$i = new PpIsect(new PpAB);
echo get_class($i->y), "\n";
try { $i->y = new class implements PpA {}; } catch (\TypeError $e) { echo "isect store TE\n"; }
?>
--EXPECT--
5
ok
union store TE
PpAB
isect store TE
--CLEAN--
<?php
