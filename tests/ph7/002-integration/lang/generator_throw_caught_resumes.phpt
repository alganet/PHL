--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() at a yield inside a try is caught by the generator's own catch and the body resumes
--FILE--
<?php
function g() {
    try {
        echo "before\n";
        yield 1;
        echo "unreached\n";
    } catch (RuntimeException $e) {
        echo "caught: ", $e->getMessage(), "\n";
    }
    echo "resumed\n";
    yield 2;
    echo "body-done\n";
}
$g = g();
echo "first=", $g->current(), "\n";
$r = $g->throw(new RuntimeException("boom"));
echo "throw-returned=", var_export($r, true), "\n";
echo "valid=", var_export($g->valid(), true), "\n";
$g->next();
echo "valid-after=", var_export($g->valid(), true), "\n";
?>
--EXPECT--
first=before
1
caught: boom
resumed
throw-returned=2
valid=true
body-done
valid-after=false
