--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a label name may be reused in every function: labels are scoped per function
--FILE--
<?php
// the plain case: the same label name declared in two functions
function glsA() { goto done; echo "unreachable;"; done: return "a"; }
function glsB() { goto done; echo "unreachable;"; done: return "b"; }
echo glsA(), glsB(), "\n";
// ...and at file scope alongside them
goto done;
echo "unreachable;";
done:
echo "top\n";
// methods of the same class, and a static one
class GlsCls {
    function m() { goto p; echo "unreachable;"; p: return "m"; }
    function n() { goto p; echo "unreachable;"; p: return "n"; }
    static function s() { goto p; echo "unreachable;"; p: return "s"; }
}
$o = new GlsCls();
echo $o->m(), $o->n(), GlsCls::s(), "\n";
// closures each carry their own label set
$f = function () { goto z; echo "unreachable;"; z: return "f"; };
$g = function () { goto z; echo "unreachable;"; z: return "g"; };
function glsZ() { goto z; echo "unreachable;"; z: return "h"; }
echo $f(), $g(), glsZ(), "\n";
// a label declared inside a LOOP in one function does not constrain a goto in another
// (jumping into a loop is the one restriction php has, and it is per function too)
function glsLoop() { while (1) { l: echo "in;"; break; } return "w"; }
function glsFlat() { goto l; echo "unreachable;"; l: return "v"; }
echo glsLoop(), glsFlat(), "\n";
// declared in a try body here, in a catch body there
function glsTry() { try { goto e; } catch (Exception $ex) {} e: return "t"; }
function glsCatch() {
    try { throw new Exception("x"); }
    catch (Exception $ex) { goto e; }
    e: return "c";
}
echo glsTry(), glsCatch(), "\n";
// a generator body is its own function too
function glsGen() { goto y; echo "unreachable;"; y: yield "gen"; }
foreach (glsGen() as $v) { echo $v, "\n"; }
// the label of a nested closure is NOT visible to the function around it,
// so this one really is a jump forward to the enclosing function's own label
function glsNested() {
    $c = function () { s: return "inner"; };
    echo $c(), ";";
    goto s;
    echo "unreachable;";
    s: return "outer";
}
echo glsNested(), "\n";
?>
--EXPECT--
ab
top
mns
fgh
in;wv
tc
gen
inner;outer
--CLEAN--
<?php
