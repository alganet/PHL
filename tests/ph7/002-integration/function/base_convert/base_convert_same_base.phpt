--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert with same from and to base
--FILE--
<?php
echo base_convert("123", 10, 10) . "\n";
echo base_convert("ff", 16, 16) . "\n";
echo base_convert("1010", 2, 2) . "\n";
echo base_convert("77", 8, 8) . "\n";
?>
--EXPECT--
123
ff
1010
77
--CLEAN--
<?php

