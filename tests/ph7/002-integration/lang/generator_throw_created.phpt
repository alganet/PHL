--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() on a never-started generator runs it to the first yield then injects there
--FILE--
<?php
// Caught at the first yield of an unstarted generator.
function g() {
    echo "started\n";
    try {
        yield 1;
    } catch (Exception $e) {
        echo "caught: ", $e->getMessage(), "\n";
    }
    yield 9;
}
$g = g();
$r = $g->throw(new Exception("early"));
echo "re-yield=", var_export($r, true), "\n";

// Unstarted generator with no try around the first yield: propagate to caller.
function h() { echo "h-started\n"; yield 1; }
$h = h();
try {
    $h->throw(new RuntimeException("nope"));
} catch (RuntimeException $e) {
    echo "caller-caught: ", $e->getMessage(), "\n";
}
echo "h-valid=", var_export($h->valid(), true), "\n";
?>
--EXPECT--
started
caught: early
re-yield=9
h-started
caller-caught: nope
h-valid=false
