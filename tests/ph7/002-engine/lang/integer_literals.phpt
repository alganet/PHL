--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Integer literals
--FILE--
<?php
echo 123 . "\n";
echo -456 . "\n";
echo 0 . "\n";
echo 7890 . "\n";
?>
--EXPECT--
123
-456
0
7890