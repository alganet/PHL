--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A send()-resumed generator's unwinding finally swallows its own inline-catch exception, and the outer exception still propagates to the caller (Face 2)
--FILE--
<?php
class A extends Exception {}
class B extends Exception {}
function g(){
    try {
        $x = yield 1;
        echo "resumed with ", $x, "\n";
        throw new A("outer");
    } finally {
        try {
            throw new B("inner");
        } catch (B $e) {
            echo "finally swallowed ", $e->getMessage(), "\n";
        }
    }
}
$gen = g();
echo "first ", $gen->current(), "\n";
try {
    $gen->send("R");
} catch (A $e) {
    echo "caught outer ", $e->getMessage(), "\n";
}
echo "done\n";
?>
--EXPECT--
first 1
resumed with R
finally swallowed inner
caught outer outer
done
--CLEAN--
<?php
