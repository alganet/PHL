--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() at a yield in a nested try: the inner catch is skipped and the outer matching catch handles it
--FILE--
<?php
function g() {
    try {
        try {
            yield 1;
            echo "unreached-inner\n";
        } catch (TypeError $e) {
            echo "inner-wrong\n";
        }
        echo "unreached-between\n";
    } catch (RuntimeException $e) {
        echo "outer-caught: ", $e->getMessage(), "\n";
    }
    yield 2;
}
$g = g();
echo "cur=", $g->current(), "\n";
$r = $g->throw(new RuntimeException("nest"));
echo "re-yield=", $r, "\n";
?>
--EXPECT--
cur=1
outer-caught: nest
re-yield=2
