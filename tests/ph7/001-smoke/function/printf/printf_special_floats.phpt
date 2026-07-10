--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
printf/sprintf special floats: INF/-INF/NaN across all conversions, width ignored
--FILE--
<?php
printf("%f|%e|%g|%E|%G|%F\n", INF, INF, INF, INF, INF, INF);
printf("%f|%e|%g|%E|%G\n", -INF, -INF, -INF, -INF, -INF);
printf("%f|%e|%g|%E|%G\n", NAN, NAN, NAN, NAN, NAN);
printf("[%10f][%-10f][%010f][%10.2f][%+f]\n", INF, INF, INF, NAN, INF);
printf("[%10e][%010g][%+e][%-8g]\n", NAN, -INF, NAN, NAN);
echo sprintf("INF %%f: %f", INF), "\n";
echo sprintf("NAN %%e: %e", NAN), "\n";
echo sprintf("-INF %%g: %g", -INF), "\n";
--EXPECT--
INF|INF|INF|INF|INF|INF
-INF|-INF|-INF|-INF|-INF
NaN|NaN|NaN|NaN|NaN
[INF][INF][INF][NaN][INF]
[NaN][-INF][NaN][NaN]
INF %f: INF
NAN %e: NaN
-INF %g: -INF
--CLEAN--
<?php
