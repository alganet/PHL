--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7: base_convert octal starting with zero

--FILE--
<?php
// Test base_convert with octal numbers starting with zero
// This tests the switch case break statements (lines 1095-1096) in builtin.c
echo base_convert('7', 8, 10) . "\n";       // Hit case 8, break at 1095
echo base_convert('123', 8, 10) . "\n";     // Hit case 8, break at 1095
echo base_convert('a', 16, 10) . "\n";      // Hit case 16, break
echo base_convert('1010', 2, 10) . "\n";    // Hit case 2, break
?>
--EXPECTF--
7
83
10
10
--CLEAN--
<?php

