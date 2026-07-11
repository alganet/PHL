--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
decbin prints the full 64-bit two's-complement for negatives
--FILE--
<?php
echo decbin(-2) . "\n";           // 62 ones then a zero (64-bit)
echo decbin(PHP_INT_MAX) . "\n";  // 63 ones
echo decbin(5) . "\n";            // 101 (positive control)
?>
--EXPECT--
1111111111111111111111111111111111111111111111111111111111111110
111111111111111111111111111111111111111111111111111111111111111
101
--CLEAN--
<?php
