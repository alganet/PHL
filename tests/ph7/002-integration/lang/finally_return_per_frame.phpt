--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A pending catch/finally return is keyed to its own body frame (per-frame slot)
--FILE--
<?php
// The deferred catch/finally `return` now lives on the body frame it returns
// from, not a single VM-global. A throw caught *within* a finally must not wipe
// an unrelated body's pending return, and a finally-over-catch override must
// survive a function call in between.

// 1) inline try/catch INSIDE a finally that catches an exception must NOT drop
//    the function's pending catch-return.
function a() {
    try { throw new Exception("a"); }
    catch (Exception $e) { return "A"; }
    finally {
        try { throw new Exception("inner"); }
        catch (Exception $e) { echo "swallowed|"; }
    }
}
echo a(), "\n";

// 2) same as (1) but the inner exception comes from a helper call.
function boom() { throw new Exception("e"); }
function b() {
    try { throw new Exception("a"); }
    catch (Exception $e) { return "B"; }
    finally {
        try { boom(); }
        catch (Exception $e) { echo "swallowed2|"; }
    }
}
echo b(), "\n";

// 3) a finally-return overrides a catch-return even with a function call in the
//    finally before the return (per-frame isolation: the helper's own return
//    targets the helper's frame, not ours).
function helper() { return 99; }
function c() {
    try { throw new Exception("a"); }
    catch (Exception $e) { return "catch"; }
    finally { helper(); return "finally"; }
}
echo c(), "\n";

// 4) an exception thrown in a finally and caught by an ENCLOSING try in the SAME
//    function whose catch returns — that return wins.
function d() {
    try {
        try { throw new Exception("a"); }
        catch (Exception $e) { return "X"; }
        finally { throw new Exception("B"); }
    } catch (Exception $e) { return "D"; }
}
echo d(), "\n";

// 5) the inner finally's throw is caught by an enclosing try whose catch does
//    NOT return; the function then falls through to its own later return.
function e() {
    try {
        try { throw new Exception("a"); }
        catch (Exception $e) { return "X"; }
        finally { throw new Exception("B"); }
    } catch (Exception $e) { echo "caught|"; }
    return "E";
}
echo e(), "\n";
?>
--EXPECT--
swallowed|A
swallowed2|B
finally
D
caught|E
--CLEAN--
<?php
