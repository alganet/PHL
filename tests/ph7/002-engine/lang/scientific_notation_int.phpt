--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Scientific notation for integers (e.g., 2e3)
--FILE--
<?php
$a = 2e3;
$b = 5E2;
$c = 1e+4;
$d = 3E-2;

if ($a == 2000) {
    echo "2e3 is 2000\n";
}
if ($b == 500) {
    echo "5E2 is 500\n";
}
if ($c == 10000) {
    echo "1e+4 is 10000\n";
}
if ($d == 0.03) {
    echo "3E-2 is 0.03\n";
}
?>
--EXPECT--
2e3 is 2000
5E2 is 500
1e+4 is 10000
3E-2 is 0.03