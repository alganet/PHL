--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
break out of a catch: levels, finally ordering, nested try, and every callable shape
--FILE--
<?php
// break 2 leaves both loops
foreach ([[1, 2], [3, 4]] as $row) {
    foreach ($row as $v) {
        try {
            if ($v == 3) { throw new Exception("x"); }
            echo "v$v;";
        } catch (Exception $e) { echo "c$v;"; break 2; }
    }
}
echo "\n";
// plain break leaves only the inner loop
foreach ([1, 2] as $a) {
    foreach ([1, 2] as $b) {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "c$a$b;"; break; }
    }
}
echo "\n";
// the try's own finally runs BEFORE the loop is left
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "c$v;"; break; }
    finally { echo "f$v;"; }
}
echo "\n";
// a try opened inside the catch body runs its finally on the way out
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) {
        echo "c$v;";
        try { echo "i;"; } finally { echo "if;"; }
        break;
    }
}
echo "\n";
// a nested try: the inner catch's break crosses the outer try too
foreach ([1, 2, 3] as $v) {
    try {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "in$v;"; break; }
    } catch (Exception $e2) { echo "out;"; }
}
echo "\n";
// the ENCLOSING try's finally must still run, before the loop is left
foreach ([1, 2, 3] as $v) {
    try {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "ic$v;"; break; }
        finally { echo "if$v;"; }
    } finally { echo "of$v;"; }
}
echo "\n";
// a break crossing TWO catch bodies: it travels out through both landing pads
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("a"); }
    catch (Exception $e1) {
        echo "o$v;";
        try { throw new Exception("b"); }
        catch (Exception $e2) { echo "i$v;"; break; }
    }
}
echo "\n";
// a return in a catch still wins over a later iteration's break
function bclFn() {
    foreach ([1, 2, 3] as $v) {
        try { throw new Exception("x"); }
        catch (Exception $e) {
            if ($v == 2) { return "r$v"; }
            echo "c$v;";
        }
    }
    return "no";
}
echo bclFn(), "\n";
// a finally that returns supersedes the break
function bclFin() {
    foreach ([1, 2, 3] as $v) {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "c$v;"; break; }
        finally { echo "f$v;"; return "rf"; }
    }
    return "no";
}
echo bclFin(), "\n";
// method, closure and generator bodies
class BclCls {
    function m() {
        foreach ([1, 2, 3] as $v) {
            try { throw new Exception("x"); }
            catch (Exception $e) { echo "m$v;"; break; }
        }
        return "done";
    }
}
$o = new BclCls();
echo $o->m(), "\n";
$fn = function () {
    foreach ([1, 2, 3] as $v) {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "cl$v;"; break; }
    }
    return "cl";
};
echo $fn(), "\n";
function bclGen() {
    foreach ([1, 2, 3] as $v) {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "g$v;"; break; }
        yield $v;
    }
    yield 99;
}
foreach (bclGen() as $x) { echo "y$x;"; }
echo "\n";
// an object iterator is left just as cleanly
foreach (new ArrayObject([1, 2, 3]) as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "ao$v;"; break; }
}
echo "\n";
?>
--EXPECT--
v1;v2;c3;
c11;c21;
c1;f1;
c1;i;if;
in1;
ic1;if1;of1;
o1;i1;
c1;r2
c1;f1;rf
m1;done
cl1;cl
g1;y99;
ao1;
--CLEAN--
<?php
