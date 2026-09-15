--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Object properties iterate base-declared first, then own, then trait members
--DESCRIPTION--
Regression: inherited properties were appended AFTER the subclass's own, so every
object-iteration consumer (var_dump, get_object_vars, foreach, (array), json_encode)
reported them back to front. php orders an instance's properties base-first, keeps an
overridden property at the BASE's position (with the child's value), and flattens trait
members last regardless of where `use` appears.
--FILE--
<?php
class B3 { public $a = 1; public $b = 2; }
class C3 extends B3 { public $c = 3; }
class S3 { public $x = 1; public $x2 = 11; }
class S4 extends S3 { public $y = 2; }
class S5 extends S4 { public $z = 3; }
trait TA { public $t1 = 1; }
class P1 { public $p = 0; }
class UseFirst extends P1 { use TA; public $own = 2; }
class UseLast extends P1 { public $own = 2; use TA; }
class G { public $g1 = 1; public $g2 = 2; }
class H extends G { public $h = 3; public $g1 = 9; }

foreach (['C3', 'S5', 'UseFirst', 'UseLast', 'H'] as $c) {
    echo str_pad($c, 9), implode(',', array_keys(get_object_vars(new $c()))), "\n";
}
$h = new H();
echo "override-value=", $h->g1, "\n";
echo "json=", json_encode(new S5()), "\n";
$k = []; foreach (new C3() as $key => $v) { $k[] = $key; }
echo "foreach=", implode(',', $k), "\n";
echo "cast=", implode(',', array_keys((array) new C3())), "\n";
?>
--EXPECT--
C3       a,b,c
S5       x,x2,y,z
UseFirst p,own,t1
UseLast  p,own,t1
H        g1,g2,h
override-value=9
json={"x":1,"x2":11,"y":2,"z":3}
foreach=a,b,c
cast=a,b,c
