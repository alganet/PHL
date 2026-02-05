--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
log function
--FILE--
<?php
echo log(1) . "\n";
echo round(log(2.718281828)) . "\n";
echo log10(10) . "\n";
?>
--EXPECT--
0
1
1
--CLEAN--
<?php
// No variables to clean

