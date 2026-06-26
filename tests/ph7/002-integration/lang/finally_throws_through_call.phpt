--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A finally that throws via a nested call (after a catch/try return) unwinds cleanly
--FILE--
<?php
// Regression: a finally mini-program that calls a function which throws past it
// used to set the interpreter pc out of the mini-program's bytecode array (the
// enclosing try's landing-pad index), corrupting VM state -> non-deterministic
// hang/crash. Each case below must now unwind deterministically and match PHP.
function helper() { throw new Exception("h"); }
function safe()   { try { throw new Exception("h"); } catch (Exception $e) {} return 1; }

// 1) catch returns, then finally calls a helper that throws uncaught -> the
//    escaping exception discards the pending return and propagates to the caller.
function a() {
    try { throw new Exception("x"); }
    catch (Exception $e) { return "A"; }
    finally { helper(); }
}
try { echo a(); } catch (Exception $e) { echo "caught:", $e->getMessage(), "\n"; }

// 2) try returns (no exception), then finally throws via a helper.
function b() {
    try { return "B"; }
    finally { helper(); }
}
try { echo b(); } catch (Exception $e) { echo "caught:", $e->getMessage(), "\n"; }

// 3) the finally's exception is caught by an ENCLOSING try in the SAME function;
//    execution then continues past it.
function c() {
    try {
        try { throw new Exception("x"); }
        catch (Exception $e) { return "C"; }
        finally { helper(); }
    } catch (Exception $e) { echo "inner:", $e->getMessage(), "\n"; }
    return "done\n";
}
echo c();

// 4) the called helper catches its own exception (nothing escapes the finally),
//    so the pending catch-return survives.
function d() {
    try { throw new Exception("x"); }
    catch (Exception $e) { return "D\n"; }
    finally { safe(); }
}
echo d();

// 5) a try/finally where the finally throws, wrapped in an outer try/catch of the
//    SAME function: the outer catch runs in place and execution CONTINUES past it
//    to the function's real return (must not unwind the whole function early).
function e() {
    try {
        try { echo "in\n"; }
        finally { helper(); }
    } catch (Exception $ex) { echo "ecaught\n"; }
    echo "after\n";
    return "E\n";
}
echo e();

// 6) like (5) but the inner try issues a `return` before the throwing finally —
//    the escaping exception discards that return; the outer catch (no return)
//    runs and the function falls through to its own later return.
function g() {
    try {
        try { return "inner"; }
        finally { helper(); }
    } catch (Exception $ex) { /* no return */ }
    return "G\n";
}
echo g();
?>
--EXPECT--
caught:h
caught:h
inner:h
done
D
in
ecaught
after
E
G
--CLEAN--
<?php
