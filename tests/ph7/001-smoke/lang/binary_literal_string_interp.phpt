--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal in string interpolation context
--FILE--
<?php
$bin = 0b1111;
echo "value=$bin\n";
echo "doubled=" . ($bin * 2) . "\n";
echo "hex=" . dechex($bin) . "\n";
?>
--EXPECT--
value=15
doubled=30
hex=f
--CLEAN--
<?php

