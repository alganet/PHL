--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
printf float conversions match PHP byte-for-byte: exponents, %g shapes, rounding, precision cap
--FILE--
<?php
$restore = set_error_handler(function ($errno, $errstr) {
    echo "[$errno] $errstr\n";
    return true;
});
// php prints exponents without zero padding, unlike C's printf
printf("%e|%E|%.2e|%.0e\n", 12345.6789, 0.00012345, 999999.5, 9.5);
// php's %g keeps a fractional digit on exponent-form output
printf("%g|%G|%.10g|%g\n", 1e20, 1e-9, 1e15, 1000000.0);
printf("%g|%g|%g|%g\n", 123456789.0, 0.000123456, 999999.0, 0.0001);
// correctly-rounded digits, full expansion at extreme magnitudes
printf("%.0f|%.0f|%.1f|%.1f\n", 0.5, 1.5, 0.25, 0.35);
echo strlen(sprintf("%f", 1e308)), "|", substr(sprintf("%f", 1e308), 0, 30), "\n";
printf("%.17e\n", 5.58924446885297306e+200);
// negative zero: %f/%e drop the sign, %g keeps it
printf("%f|%e|%g\n", -0.0, -0.0, -0.0);
// zero padding goes between the sign and the digits
printf("[%015.4e][%010.2f][%-12.3e]\n", -12345.6789, -1.5, -12345.6789);
// precision is capped at 53 with a notice
echo sprintf("%.60f", 0.1), "\n";
// extreme literals parse correctly rounded (denormals, overflow to INF)
var_export(5e-324 > 0.0); echo "\n";
var_export(1e400); echo "\n";
var_export((float)"1e400"); echo "\n";
printf("%G|%g\n", 2.2250738585072014e-308, 5e-324);
var_export((float)"5.58924446885297306e+200" === 5.58924446885297306e+200); echo "\n";
set_error_handler($restore);
--EXPECT--
1.234568e+4|1.234500E-4|1.00e+6|1e+1
1.0e+20|1.0E-9|1.0e+15|1.0e+6
1.23457e+8|0.000123456|999999|0.0001
0|2|0.2|0.3
316|100000000000000001097906362944
5.58924446885297306e+200
0.000000|0.000000e+0|-0
[-000001.2346e+4][-000001.50][-1.235e+4   ]
[8] sprintf(): Requested precision of 60 digits was truncated to PHP maximum of 53 digits
0.10000000000000000555111512312578270211815834045410156
true
INF
INF
2.22507E-308|4.94066e-324
true
--CLEAN--
<?php
