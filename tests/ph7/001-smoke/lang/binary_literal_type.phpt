--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Binary literal type is integer
--FILE--
<?php
echo is_int(0b0) ? "true" : "false";
echo "\n";
echo is_int(0b1010) ? "true" : "false";
echo "\n";
echo is_int(0B11111111) ? "true" : "false";
echo "\n";
echo is_integer(0b1010) ? "true" : "false";
echo "\n";
echo is_numeric(0b1010) ? "true" : "false";
echo "\n";
echo is_float(0b1010) ? "false" : "true";
echo "\n";
echo is_string(0b1010) ? "false" : "true";
echo "\n";
?>
--EXPECT--
true
true
true
true
true
true
true
--CLEAN--
<?php

