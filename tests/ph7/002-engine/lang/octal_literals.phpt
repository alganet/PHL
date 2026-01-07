--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test octal literals
--FILE--
<?php
echo 077 . "\n"; // 63
echo 010 . "\n"; // 8
echo 0 . "\n"; // 0
?>
--EXPECT--
63
8
0