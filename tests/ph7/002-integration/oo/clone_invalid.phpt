--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Cloning a non-object throws a catchable TypeError (PHP 8)
--FILE--
<?php
// Cloning a scalar / null throws TypeError in PHP 8 (was a warning-and-null
// PH7-ism). Catch + getMessage() to stay cross-engine (dodges the uncaught
// trace-format divergence). Both the `clone $x` and clone($x) forms apply.
$scalar = 42;
try {
    $cloned = clone $scalar;
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
$null = null;
try {
    $cloned2 = clone $null;
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
// call form
try {
    $cloned3 = clone("string");
} catch (\TypeError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
clone(): Argument #1 ($object) must be of type object, int given
clone(): Argument #1 ($object) must be of type object, null given
clone(): Argument #1 ($object) must be of type object, string given
--CLEAN--
<?php
