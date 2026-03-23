--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert is case insensitive for input
--FILE--
<?php
echo base_convert("FF", 16, 10) . "\n";
echo base_convert("ff", 16, 10) . "\n";
echo base_convert("Ff", 16, 10) . "\n";
echo base_convert("HELLO", 36, 10) . "\n";
echo base_convert("hello", 36, 10) . "\n";
?>
--EXPECT--
255
255
255
29234652
29234652
--CLEAN--
<?php

