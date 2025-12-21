--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Parse unary to binary operator conversion with alpha-stream variable names
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test expression starting with + or - (unary operators, parse.c lines 362-363)
$result = +5;
echo "Unary plus: " . ($result === 5 ? "OK" : "FAIL") . "\n";

$result = -3;
echo "Unary minus: " . ($result === -3 ? "OK" : "FAIL") . "\n";

// Test alpha-stream operator variable names (ticket 1433-013, parse.c lines 472-491)
// Variables named like operators should work correctly with +/- that follows
$and = 10;
$or = 20;
$xor = 30;

// These test unary +/- after a variable being converted to binary
$result = $and +5;
echo "and+5: " . ($result === 15 ? "OK" : "FAIL") . "\n";

$result = $or -8;
echo "or-8: " . ($result === 12 ? "OK" : "FAIL") . "\n";

$result = $xor +$and;
echo "xor+and: " . ($result === 40 ? "OK" : "FAIL") . "\n";

// Mixed expressions
$eq = 100;
$result = $eq +$and -$or;
echo "eq+and-or: " . ($result === 90 ? "OK" : "FAIL") . "\n";

echo "Done\n";
?>
--EXPECT--
Unary plus: OK
Unary minus: OK
and+5: OK
or-8: OK
xor+and: OK
eq+and-or: OK
Done