--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Math functions
--FILE--
<?php
echo round(3.14159, 2); // 3.14
echo "\n";
echo max(1, 3, 2); // 3
echo "\n";
echo min(1, 3, 2); // 1
?>
--EXPECT--
3.14
3
1
--CLEAN--
<?php
// No variables to clean
?>