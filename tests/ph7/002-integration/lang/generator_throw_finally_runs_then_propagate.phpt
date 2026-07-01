--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() at a yield inside try/finally (no catch) runs the finally then propagates to the caller
--FILE--
<?php
function g() {
    try {
        yield 1;
        echo "unreached\n";
    } finally {
        echo "finally\n";
    }
    echo "unreached-2\n";
}
$g = g();
echo "cur=", $g->current(), "\n";
try {
    $g->throw(new Exception("Z"));
} catch (Exception $e) {
    echo "caller-caught: ", $e->getMessage(), "\n";
}
echo "valid=", var_export($g->valid(), true), "\n";
?>
--EXPECT--
cur=1
finally
caller-caught: Z
valid=false
