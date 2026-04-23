--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Exponentiation operator ** (PHP 5.6)
--FILE--
<?php
echo 2 ** 10, "\n";
echo 2 ** 0, "\n";
echo 0 ** 0, "\n";
echo 0 ** 5, "\n";
echo 1 ** 50, "\n";
echo 10 ** 3, "\n";
echo 2 ** 3 ** 2, "\n";
echo (2 ** 3) ** 2, "\n";
echo 2.5 ** 2, "\n";
echo 2 ** -2, "\n";
echo (-2) ** 3, "\n";
echo (-2) ** 2, "\n";
echo 2 ** 3, "\n";
echo is_int(2 ** 3) ? "int\n" : "float\n";
echo 3 ** 4, "\n";
echo is_int(3 ** 4) ? "int\n" : "float\n";
?>
--EXPECT--
1024
1
1
0
1
1000
512
64
6.25
0.25
-8
4
8
int
81
int
