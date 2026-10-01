--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exceptions: a try inside a catch body must not tear down the catch's own frame
--FILE--
<?php
/* A catch body runs as a mini-program entered on a transparent frame that
 * carries VM_FRAME_EXCEPTION. A try written INSIDE that catch body ended by
 * popping that frame -- the one its own exec was entered on -- and the body's
 * terminal instruction then read it back after it had been freed. */

class FrTh
{
    public function boom() { throw new RuntimeException('thrown'); }
}

function frNested()
{
    try {
        (new FrTh)->boom();
    } catch (RuntimeException $e) {
        try { (new FrTh)->boom(); } catch (RuntimeException $inner) { echo "inner: ", $inner->getMessage(), "\n"; }
        echo "outer: ", $e->getMessage(), "\n";
    }
    return 'returned';
}

function frNestedReturns()
{
    /* The freed frame is what carries a catch body's pending `return`. */
    try {
        (new FrTh)->boom();
    } catch (RuntimeException $e) {
        try { (new FrTh)->boom(); } catch (RuntimeException $inner) { }
        return 'from the catch';
    }
    return 'unreachable';
}

function frNestedFinally()
{
    try {
        (new FrTh)->boom();
    } catch (RuntimeException $e) {
        try { (new FrTh)->boom(); } catch (RuntimeException $inner) { } finally { echo "inner finally\n"; }
        echo "after inner finally\n";
    } finally {
        echo "outer finally\n";
    }
    return 'done';
}

function frNestedUncaughtInner()
{
    /* The inner try does not match: the throw leaves the catch body outward. */
    try {
        try {
            (new FrTh)->boom();
        } catch (RuntimeException $e) {
            try { (new FrTh)->boom(); } catch (LogicException $no) { echo "unreachable\n"; }
            echo "unreachable too\n";
        }
    } catch (RuntimeException $outer) {
        echo "escaped: ", $outer->getMessage(), "\n";
    }
    return 'escaped-done';
}

echo frNested(), "\n";
echo frNestedReturns(), "\n";
echo frNestedFinally(), "\n";
echo frNestedUncaughtInner(), "\n";
?>
--EXPECT--
inner: thrown
outer: thrown
returned
from the catch
inner finally
after inner finally
outer finally
done
escaped: thrown
escaped-done
--CLEAN--
<?php
unset($e,$inner,$no,$outer);
