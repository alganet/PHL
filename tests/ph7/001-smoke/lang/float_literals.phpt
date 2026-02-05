--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Float/real number literals processing
--FILE--
<?php
// Test float/real number literals
// Covers PH7_CompileNumLiteral else branch (lines ~333, 341, 395-396)

$float1 = 3.14;
$float2 = 2.71828;
$float3 = 1.0e10;
$float4 = 5.67E-3;

echo $float1 . "\n";
echo $float2 . "\n";
echo $float3 . "\n";
echo $float4 . "\n";

echo "Done\n";
?>
--EXPECTF--
3.14
2.71828
10000000000
0.00567
Done
--CLEAN--
<?php
unset($float1, $float2, $float3, $float4);
