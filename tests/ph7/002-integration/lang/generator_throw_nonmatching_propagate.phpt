--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() with a catch whose type does not match propagates to the caller and closes the generator
--FILE--
<?php
function g() {
    try {
        yield 1;
    } catch (TypeError $e) {
        echo "wrong-branch\n";
    }
    echo "unreached\n";
}
$g = g();
echo "cur=", $g->current(), "\n";
try {
    $g->throw(new RuntimeException("Y"));
} catch (RuntimeException $e) {
    echo "caller-caught: ", get_class($e), ":", $e->getMessage(), "\n";
}
echo "valid=", var_export($g->valid(), true), "\n";
?>
--EXPECT--
cur=1
caller-caught: RuntimeException:Y
valid=false
