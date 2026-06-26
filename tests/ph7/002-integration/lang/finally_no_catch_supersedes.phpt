--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A finally on a try with no matching catch can supersede the in-flight exception
--FILE--
<?php
// On the no-catch path a finally that issues a `return` swallows the in-flight
// exception, and a finally that throws supersedes it; previously the original
// exception was re-thrown regardless (leaked as a spurious uncaught / lost return).

// 1) finally `return` swallows the exception (no catch on the try).
function a() {
    try { throw new Exception("A"); }
    finally { return "A-ret\n"; }
}
echo a();

// 2) same, but the call sits inside an outer try (the body must RETURN, not unwind).
function b() {
    try { throw new Exception("A"); }
    finally { return "B-ret"; }
}
try { echo b(), "\n"; } catch (Exception $e) { echo "unexpected\n"; }

// 3) finally throws; the new exception is the one the outer catch sees.
function c() {
    try { throw new Exception("orig"); }
    finally { throw new Exception("from-finally"); }
}
try { c(); } catch (Exception $e) { echo "c:", $e->getMessage(), "\n"; }

// 4) finally throws via a nested call; same result.
function boom() { throw new Exception("via-call"); }
function d() {
    try { throw new Exception("orig"); }
    finally { boom(); }
}
try { d(); } catch (Exception $e) { echo "d:", $e->getMessage(), "\n"; }

// 5) finally's throw caught by an enclosing try of the SAME function; execution
//    continues to the real return afterwards.
function e() {
    try {
        try { throw new Exception("orig"); }
        finally { throw new Exception("inner"); }
    } catch (Exception $ex) { echo "e:", $ex->getMessage(), "\n"; }
    return "e-end\n";
}
echo e();

// 6) control: a finally that completes normally still propagates the original.
function f() {
    try { throw new Exception("kept"); }
    finally { echo "f-finally\n"; }
}
try { f(); } catch (Exception $e) { echo "f:", $e->getMessage(), "\n"; }
?>
--EXPECT--
A-ret
B-ret
c:from-finally
d:via-call
e:inner
e-end
f-finally
f:kept
--CLEAN--
<?php
