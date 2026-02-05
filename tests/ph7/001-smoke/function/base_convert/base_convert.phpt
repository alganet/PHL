--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert converts between bases
--FILE--
<?php
echo base_convert('a', 16, 10) . "\n"; // 10
echo base_convert('1010', 2, 10) . "\n"; // 10
echo base_convert('10', 8, 10) . "\n"; // 10
?>
--EXPECT--
10
10
8
--CLEAN--
<?php

