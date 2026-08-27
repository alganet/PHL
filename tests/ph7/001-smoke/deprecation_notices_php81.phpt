--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
php 8.1 deprecation notices: null-to-string ZPP args, lossy float->int coercion
--FILE--
<?php
set_error_handler(function ($no, $str) { echo "[$no] $str\n"; return true; });
echo strlen(null), "\n";
echo substr(null, 0), "|\n";
echo strtoupper(null), "|\n";
echo trim(null), "|\n";
echo strtolower(null), "|\n";
echo ltrim(null), "|\n";
echo rtrim(null), "|\n";
function dnpF(int $x) { return $x; }
echo dnpF(3.5), "\n";
echo dnpF(4.0), "\n"; // integral float: no notice
?>
--EXPECT--
[8192] strlen(): Passing null to parameter #1 ($string) of type string is deprecated
0
[8192] substr(): Passing null to parameter #1 ($string) of type string is deprecated
|
[8192] strtoupper(): Passing null to parameter #1 ($string) of type string is deprecated
|
[8192] trim(): Passing null to parameter #1 ($string) of type string is deprecated
|
[8192] strtolower(): Passing null to parameter #1 ($string) of type string is deprecated
|
[8192] ltrim(): Passing null to parameter #1 ($string) of type string is deprecated
|
[8192] rtrim(): Passing null to parameter #1 ($string) of type string is deprecated
|
[8192] Implicit conversion from float 3.5 to int loses precision
3
4
--CLEAN--
<?php
