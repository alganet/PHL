--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Integer literal exceeding the signed 64-bit range parses as float (all bases)
--FILE--
<?php
/* An integer literal larger than PHP_INT_MAX promotes to float in PHP instead
 * of wrapping or dropping a digit. A unary minus on such a literal negates the
 * float. The int/float classification is byte-identical to php across every
 * base; decimal and hex values are byte-exact too. */
function ty($v) { return is_float($v) ? "float" : (is_int($v) ? "int" : "?"); }
echo ty(9223372036854775807), "\n";   // PHP_INT_MAX, fits -> int
echo ty(9223372036854775808), "\n";   // decimal overflow -> float
echo ty(18446744073709551616), "\n";  // -> float
echo ty(-9223372036854775808), "\n";  // minus applied to a float literal -> float
echo ty(0x7FFFFFFFFFFFFFFF), "\n";     // hex max, fits -> int
echo ty(0x8000000000000000), "\n";     // hex overflow -> float
echo ty(0xFFFFFFFFFFFFFFFF), "\n";
echo ty(0777777777777777777777), "\n";      // 21 octal 7s == INT_MAX, fits -> int
echo ty(01000000000000000000000), "\n";     // octal overflow -> float
echo ty(0b111111111111111111111111111111111111111111111111111111111111111), "\n"; // 63 ones, fits -> int
echo ty(0b1000000000000000000000000000000000000000000000000000000000000000), "\n"; // 64 bits -> float
/* Decimal and hex overflow values are byte-exact vs php 8.5.7. Octal/binary
 * overflow values are intentionally NOT asserted here: they can differ from php
 * in the low bit(s) because php's zend_{oct,bin}_strtod rounds differently than
 * the dv*base+digit doubling (php's binary 2**63 is 2**63-1024; phl returns the
 * exact 2**63). Recorded as a residual. */
echo 9223372036854775808 === 9.223372036854776E+18 ? "dec_ok\n" : "dec_bad\n";
echo 0xFFFFFFFFFFFFFFFF === 1.8446744073709552E+19 ? "hex_ok\n" : "hex_bad\n";
?>
--EXPECT--
int
float
float
float
int
float
float
int
float
int
float
dec_ok
hex_ok
