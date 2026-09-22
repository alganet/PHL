--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
goto out of a catch reaches its label, and runs the finallys it crosses
--FILE--
<?php
// out of a catch to a label after the loop
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) { echo "c$v;"; goto out1; }
}
out1:
echo "\n";
// a label INSIDE the same catch body is an ordinary jump, not an exit
foreach ([1, 2] as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) {
        echo "a$v;";
        goto in1;
        echo "unreachable;";
        in1:
        echo "b$v;";
    }
}
echo "\n";
// ...including inside a function, where the label used to read as undefined
function gocFn() {
    foreach ([1, 2] as $v) {
        try { throw new Exception("x"); }
        catch (Exception $e) {
            echo "a$v;";
            goto in2;
            echo "unreachable;";
            in2:
            echo "b$v;";
        }
    }
    return "r";
}
echo gocFn(), "\n";
// the crossed try's finally runs before the jump, outermost last
foreach ([1, 2, 3] as $v) {
    try {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "ic$v;"; goto out2; }
        finally { echo "if$v;"; }
    } finally { echo "of$v;"; }
}
out2:
echo "\n";
// crossing TWO catch bodies
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("a"); }
    catch (Exception $e1) {
        echo "o$v;";
        try { throw new Exception("b"); }
        catch (Exception $e2) { echo "i$v;"; goto out3; }
    }
}
out3:
echo "\n";
// backward goto out of a catch
$n = 0;
top:
$n++;
foreach ([1, 2, 3] as $v) {
    try { throw new Exception("x"); }
    catch (Exception $e) {
        echo "c$n$v;";
        if ($n < 3) { goto top; }
        goto out4;
    }
}
out4:
echo "\n";
// the throw comes from a nested call; method, closure and generator bodies
function gocBoom() { throw new Exception("x"); }
foreach ([1, 2, 3] as $v) {
    try { gocBoom(); }
    catch (Exception $e) { echo "d$v;"; goto out5; }
}
out5:
echo "\n";
class GocCls {
    function m() {
        foreach ([1, 2, 3] as $v) {
            try { throw new Exception("x"); }
            catch (Exception $e) { echo "m$v;"; goto mdone; }
        }
        mdone:
        return "mm";
    }
}
$o = new GocCls();
echo $o->m(), "\n";
$fn = function () {
    foreach ([1, 2, 3] as $v) {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "cl$v;"; goto cldone; }
    }
    cldone:
    return "cc";
};
echo $fn(), "\n";
function gocGen() {
    foreach ([1, 2, 3] as $v) {
        try { throw new Exception("x"); }
        catch (Exception $e) { echo "g$v;"; goto gdone; }
        yield $v;
    }
    gdone:
    yield 99;
}
foreach (gocGen() as $x) { echo "y$x;"; }
echo "\n";
?>
--EXPECT--
c1;
a1;b1;a2;b2;
a1;b1;a2;b2;r
ic1;if1;of1;
o1;i1;
c11;c21;c31;
d1;
m1;mm
cl1;cc
g1;y99;
--CLEAN--
<?php
