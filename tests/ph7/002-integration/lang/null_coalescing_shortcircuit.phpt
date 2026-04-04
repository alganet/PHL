--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Null coalescing ?? short-circuits (RHS not evaluated when LHS non-null)
--FILE--
<?php
function sideEffect() {
    echo "EVALUATED\n";
    return "rhs";
}

// LHS non-null: RHS must NOT be evaluated
$a = "hello";
$b = $a ?? sideEffect();
echo $b . "\n";

// LHS null: RHS must be evaluated
$c = null;
$d = $c ?? sideEffect();
echo $d . "\n";

// Chained: stops at first non-null
$e = null;
$f = "found";
$g = $e ?? $f ?? sideEffect();
echo $g . "\n";
?>
--EXPECT--
hello
EVALUATED
rhs
found
--CLEAN--
<?php

