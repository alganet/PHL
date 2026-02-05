--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert output to different bases
--FILE--
<?php
// Test conversion to binary (base 2)
echo base_convert('10', 10, 2) . "\n";

// Test conversion to octal (base 8)
echo base_convert('10', 10, 8) . "\n";

// Test conversion to hexadecimal (base 16)
echo base_convert('10', 10, 16) . "\n";

// Test another value
echo base_convert('255', 10, 16) . "\n";
?>
--EXPECT--
1010
12
a
ff
--CLEAN--
<?php

