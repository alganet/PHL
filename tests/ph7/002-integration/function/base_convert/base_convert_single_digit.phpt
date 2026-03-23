--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with single digit values
--FILE--
<?php
echo base_convert("1", 10, 16) . "\n";
echo base_convert("9", 10, 16) . "\n";
echo base_convert("a", 16, 10) . "\n";
echo base_convert("1", 2, 10) . "\n";
echo base_convert("7", 8, 10) . "\n";
?>
--EXPECT--
1
9
10
1
7
--CLEAN--
<?php

