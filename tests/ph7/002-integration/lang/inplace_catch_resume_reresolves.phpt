--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A catch that runs in place resumes at the catching frame's landing pad, not the nearest try
--FILE--
<?php
// ROOT B: when a `catch` runs in place at the frame that actually caught the
// exception (often several frames above the throw), control must resume at THAT
// frame's landing pad — not the lexically/stack-nearest try. Each case below
// used to resume at the wrong pad: running dead code after an inner try, falling
// through a `yield from`, or losing an outer catch's return.

// (a) an inner finally throws; the OUTER try catches it. The statement after the
//     inner try/finally is dead code and must not run.
function a() {
    try {
        try { throw new Exception("o"); }
        finally { throw new Exception("x"); }
        echo "UNREACHABLE|";
    } catch (Exception $e) {
        echo "a:", $e->getMessage(), "|";
    }
}
a();
echo "\n";

// (b) `yield from` over a sub-generator that throws; the outer generator's try
//     catches it. The statement after `yield from` must not run, and a later
//     plain `yield` still fires.
function sub() { yield 1; throw new Exception("s"); }
function g() {
    try { yield from sub(); echo "UNREACHABLE|"; }
    catch (Exception $e) { echo "b:", $e->getMessage(), "|"; }
    yield 9;
}
foreach (g() as $v) { echo "y:$v|"; }
echo "\n";

// (c) an outer function's catch issues a `return` over a finally that threw via
//     a CALL. The function resumes in the catching frame and that return wins.
function boom() { throw new Exception("B"); }
function f() {
    try { throw new Exception("a"); }
    catch (Exception $e) { return "X"; }
    finally { boom(); }
}
function caller() {
    try { return f(); }
    catch (Exception $e) { return "C"; }
}
echo caller(), "\n";

// (c2) the same, but the enclosing catch is in the SAME function (an inner
//      try/catch/finally whose finally throws via a call, wrapped by an outer
//      try/catch that returns).
function same() {
    try {
        try { throw new Exception("a"); }
        catch (Exception $e) { return "X"; }
        finally { boom(); }
    } catch (Exception $e) { return "CR"; }
}
echo same(), "\n";

// (c-scope) the in-place catch must see the catching function's locals (the
//      catch body runs in the try-owning frame, not the deeper throw site).
function withlocal() {
    $local = "L";
    try { return f(); }
    catch (Exception $e) { return $local . ":" . $e->getMessage(); }
}
echo withlocal(), "\n";

// (nested-rethrow) an inner multi-catch re-throws through a finally to an outer
//   catch. Resolving the resume at the OUTER landing must pop the inner try's
//   exception frame; otherwise a frame leaks and corrupts later execution (this
//   silently aborted a *subsequent* program in the same long-lived VM). Two trys
//   here, then a finally-return-swallows after, must both still run.
class Nr1 extends Exception {}
class Nr2 extends Exception {}
function nested() {
    try {
        try { throw new Nr2("r"); }
        catch (Nr1 | Nr2 $e) { echo "in|"; throw $e; }
        finally { echo "fin|"; }
    } catch (Nr2 $e) { echo "out|"; }
}
nested();
function swallow() {
    try { throw new Exception("E1"); }
    catch (Exception $e) { throw new RuntimeException("E2"); }
    finally { return "FIN"; }
}
echo swallow(), "\n";
?>
--EXPECT--
a:x|
y:1|b:s|y:9|
C
CR
L:B
in|fin|out|FIN
--CLEAN--
<?php
