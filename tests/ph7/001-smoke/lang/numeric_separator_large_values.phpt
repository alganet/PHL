--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator with large values
--FILE--
<?php
echo 9_223_372_036_854_775_807 . "\n";
echo 1_000_000_000_000 . "\n";
echo 0x7FFF_FFFF_FFFF_FFFF . "\n";
echo 0b1111_1111_1111_1111_1111_1111_1111_1111 . "\n";
echo (int)(9_223_372_036_854_775_807 === PHP_INT_MAX) . "\n";
echo (int)(1_000_000_000_000 === 1000000000000) . "\n";
// 63-bit binary literal with every-3-bit separator is ~85 chars, inside the
// compile-time stripper's 128-byte stack scratch.
echo 0b111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111 . "\n";
echo (int)(0b111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111_111 === PHP_INT_MAX) . "\n";
// 200-digit decimal literal with separators overflows the 128-byte stack
// scratch and exercises the stripper's heap-allocation fallback path. The
// int64 parser saturates at the representable range, which is fine — we
// only care that the long literal is accepted and stripped without crashing
// or silently dropping the underscore-rejected low bits.
echo (int)(1_234_567_890_123_456_789_012_345_678_901_234_567_890_123_456_789_012_345_678_901_234_567_890_123_456_789_012_345_678_901_234_567_890_123_456_789_012_345_678_901_234_567_890_123_456_789_012_345_678_901_234_567_890 > 0) . "\n";
?>
--EXPECT--
9223372036854775807
1000000000000
9223372036854775807
4294967295
1
1
9223372036854775807
1
1
--CLEAN--
<?php

