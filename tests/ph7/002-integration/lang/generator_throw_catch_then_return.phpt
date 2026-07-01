--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Generator::throw() caught by a catch that returns completes the generator with that return value
--FILE--
<?php
function g() {
    try {
        yield 1;
    } catch (Exception $e) {
        return 77;
    }
    yield 2;   // must NOT run: the catch returned
}
$g = g();
echo "cur=", $g->current(), "\n";
$r = $g->throw(new Exception("q"));
echo "throw-returned=", var_export($r, true), "\n";
echo "valid=", var_export($g->valid(), true), "\n";
echo "getReturn=", $g->getReturn(), "\n";
?>
--EXPECT--
cur=1
throw-returned=NULL
valid=false
getReturn=77
