--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
printf with special float values
--SKIPIF--
<?php echo 'skip'; ?>
--FILE--
<?php
// Test printf with special float values to cover edge cases in formatting
echo "INF values:\n";
printf("%%f: %f\n", INF);
printf("%%e: %e\n", INF);
printf("%%g: %g\n", INF);
printf("%%E: %E\n", INF);
printf("%%G: %G\n", INF);

echo "\n-INF values:\n";
printf("%%f: %f\n", -INF);
printf("%%e: %e\n", -INF);
printf("%%g: %g\n", -INF);
printf("%%E: %E\n", -INF);
printf("%%G: %G\n", -INF);

echo "\nNAN values:\n";
printf("%%f: %f\n", NAN);
printf("%%e: %e\n", NAN);
printf("%%g: %g\n", NAN);
printf("%%E: %E\n", NAN);
printf("%%G: %G\n", NAN);

echo "\nLarge positive values:\n";
printf("%%f: %f\n", 1e308);
printf("%%e: %e\n", 1e308);
printf("%%g: %g\n", 1e308);

echo "\nLarge negative values:\n";
printf("%%f: %f\n", -1e308);
printf("%%e: %e\n", -1e308);
printf("%%g: %g\n", -1e308);

echo "\nSmall positive values:\n";
printf("%%f: %f\n", 1e-307);
printf("%%e: %e\n", 1e-307);
printf("%%g: %g\n", 1e-307);

echo "\nSmall negative values:\n";
printf("%%f: %f\n", -1e-307);
printf("%%e: %e\n", -1e-307);
printf("%%g: %g\n", -1e-307);

echo "\nUsing sprintf:\n";
echo sprintf("INF %%f: %f\n", INF);
echo sprintf("NAN %%e: %e\n", NAN);
echo sprintf("-INF %%g: %g\n", -INF);
?>
--EXPECT--
INF values:
%f: 0.000000
%e: 0.000000e+00
%g: 0
%E: 0.000000E+00
%G: 0

-INF values:
%f: 0.000000
%e: 0.000000e+00
%g: 0
%E: 0.000000E+00
%G: 0

NAN values:
%f: 0.000000
%e: 0.000000e+00
%g: 0
%E: 0.000000E+00
%G: 0

Large positive values:
%f: 100000000000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.000000
%e: 1.000000e+308
%g: 1e+308

Large negative values:
%f: -100000000000000100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000.000000
%e: -1.000000e+308
%g: -1e+308

Small positive values:
%f: 0.000000
%e: 1.000000e-307
%g: 1e-307

Small negative values:
%f: -0.000000
%e: -1.000000e-307
%g: -1e-307

Using sprintf:
INF %f: 0.000000
NAN %e: 0.000000e+00
-INF %g: 0