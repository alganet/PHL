--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Default float-to-string conversion matches PHP (precision 14, gcvt shapes, literal parsing, json)
--FILE--
<?php
// php's default float->string conversion: precision=14, gcvt shapes
echo 0.1 + 0.2, "|", 1 / 3, "|", 2.12345678901234567890, "\n";
echo 1e15, "|", 1e14, "|", 100000000000000.0 - 1, "|", 0.0001, "|", 0.00001, "\n";
echo -0.0, "|", 1e100, "|", -1.5e-300, "|", 987654321.12345678, "\n";
echo (string)(7.0 / 11.0), "|", "" . 1e-320, "\n";
// extreme literals parse correctly rounded (denormals, overflow to INF)
echo 1e400, "|", (float)"1e400", "|", 5e-324 > 0.0 ? "denormal-ok" : "denormal-broken", "\n";
echo (float)"5.58924446885297306e+200" === 5.58924446885297306e+200 ? "roundtrip-ok" : "roundtrip-broken", "\n";
// json floats follow serialize_precision (shortest), lowercase exponent
echo json_encode([1/3, 1.0, 0.1 + 0.2, 1e15, 1e17, -0.0, 1e100, 5e-324]), "\n";
echo json_encode(["a" => "1e15", "b" => "2.5"], JSON_NUMERIC_CHECK), "\n";
// a significant digit buried past 500 fractional zeros still parses
$s = "0." . str_repeat("0", 520) . "1e300";
echo (float)$s, "\n";
--EXPECT--
0.3|0.33333333333333|2.1234567890123
1.0E+15|1.0E+14|99999999999999|0.0001|1.0E-5
-0|1.0E+100|-1.5E-300|987654321.12346
0.63636363636364|9.9998886718268E-321
INF|INF|denormal-ok
roundtrip-ok
[0.3333333333333333,1,0.30000000000000004,1000000000000000,1.0e+17,-0,1.0e+100,5.0e-324]
{"a":1000000000000000,"b":2.5}
1.0E-221
--CLEAN--
<?php
unset($s);
