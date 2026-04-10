--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Numeric literal separator in switch case labels
--FILE--
<?php
function classify($n) {
    switch ($n) {
        case 1_000:
            return "thousand";
        case 1_000_000:
            return "million";
        case 0xFF_FF:
            return "max16";
        case 0b1010_1010:
            return "pattern";
        default:
            return "other";
    }
}
echo classify(1000) . "\n";
echo classify(1_000_000) . "\n";
echo classify(0xFFFF) . "\n";
echo classify(170) . "\n";
echo classify(42) . "\n";
?>
--EXPECT--
thousand
million
max16
pattern
other
--CLEAN--
<?php

