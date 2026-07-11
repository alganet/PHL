--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
sprintf() accepts the ignored 'l' length modifier and the %h/%H (g/G) specifiers
--FILE--
<?php
// A single 'l' length modifier is a C-ism php accepts and ignores.
echo sprintf("%ld", 42), "\n";
echo sprintf("%5ld", 42), "\n";
echo sprintf("%lf", 3.5), "\n";
echo sprintf("%ls", "hi"), "\n";
// %h / %H are the (locale-independent) twins of %g / %G.
echo sprintf("%h", 0.0001), "\n";
echo sprintf("%H", 123456789.0), "\n";
echo sprintf("%.4h", 3.14159), "\n";
// A second 'l' becomes the (unknown) specifier, exactly like php.
try {
    sprintf("%lld", 42);
} catch (\ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
42
   42
3.500000
hi
0.0001
1.23457E+8
3.142
Unknown format specifier "l"
--CLEAN--
<?php
