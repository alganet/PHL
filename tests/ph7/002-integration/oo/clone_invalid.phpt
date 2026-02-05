--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Clone on invalid type should produce error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test cloning a scalar value (should return null)
$scalar = 42;
$cloned = clone $scalar;
if ($cloned === null) {
    echo "Clone scalar returned null\n";
} else {
    echo "Unexpected result\n";
}

// Test cloning null
$null = null;
$cloned2 = clone $null;
if ($cloned2 === null) {
    echo "Clone null returned null\n";
} else {
    echo "Unexpected result for null\n";
}
?>
--EXPECTF--
%s Error: Clone: Expecting a class instance as left operand,PH7 is loading NULL
Clone scalar returned null
%s Error: Clone: Expecting a class instance as left operand,PH7 is loading NULL
Clone null returned null
--CLEAN--
<?php
unset($scalar, $cloned, $null, $cloned2);
