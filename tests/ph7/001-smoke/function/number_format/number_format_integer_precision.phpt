--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
number_format() formats an integer as an integer, and prints inf/nan the way php does
--FILE--
<?php
// The embedded-PHP implementation cast every argument to float, so an INTEGER
// past 2^53 came back with its low digits replaced by zeros -- a wrong number,
// silently. php has a separate integer path for exactly this.
var_dump(number_format(123456789012345678));
var_dump(number_format(9223372036854775807));
var_dump(number_format(-9223372036854775807));
var_dump(number_format(123456789012345678, 2));
// A negative $decimals rounds the integer itself, half away from zero.
var_dump(number_format(123456789012345678, -2));
var_dump(number_format(9223372036854775807, -1));
var_dump(number_format(-9223372036854775807, -2));
var_dump(number_format(123456789012345678, -30));
var_dump(number_format(-45, -2), number_format(-49, -2));

// A double past 2^52 has no fractional digits left, so php formats it as an
// integer too -- which is what keeps this one exact.
var_dump(number_format(4503599627370496.0));
var_dump(number_format(1e20));

// INF and NAN come back from php's own printf, without a sign, a separator or
// any padding.
var_dump(number_format(INF), number_format(INF, 2), number_format(-INF, 2));
var_dump(number_format(NAN), number_format(NAN, 5));

// $decimals is not capped at the engine's printf precision.
var_dump(strlen(number_format(1.5, 100)));

// The ordinary cases are unchanged, half away from zero.
var_dump(number_format(0.5), number_format(1.5), number_format(2.5), number_format(-1.5));
var_dump(number_format(1234.5678, 2), number_format(1234.5678, 2, ',', '.'));
var_dump(number_format(1234.5678, 2, '', ''), number_format(1234567.891, 2, '<>', '__'));
var_dump(number_format(1234.5678, 2, null, null));
var_dump(number_format(0.045, 2), number_format(999999.995, 2));
var_dump(number_format(" 42", -1), number_format("1e3"));

// php's stub declares `float $num` — which is what Reflection prints — while
// the refusal it words comes from a macro that says int|float.
$nfR = new ReflectionFunction('number_format');
foreach ($nfR->getParameters() as $nfP) {
    echo $nfP->getPosition(), " ", $nfP->getType(), " $", $nfP->getName(), "\n";
}
try { number_format([1]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { number_format("abc"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { number_format("12abc"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { number_format(1.5, "2abc"); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { number_format(1.5, 2, [1]); } catch (TypeError $e) { echo $e->getMessage(), "\n"; }
try { number_format(); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
try { number_format(1, 2, '.', ',', 5); } catch (ArgumentCountError $e) { echo $e->getMessage(), "\n"; }
?>
--EXPECT--
string(23) "123,456,789,012,345,678"
string(25) "9,223,372,036,854,775,807"
string(26) "-9,223,372,036,854,775,807"
string(26) "123,456,789,012,345,678.00"
string(23) "123,456,789,012,345,700"
string(25) "9,223,372,036,854,775,810"
string(26) "-9,223,372,036,854,775,800"
string(1) "0"
string(1) "0"
string(1) "0"
string(21) "4,503,599,627,370,496"
string(27) "100,000,000,000,000,000,000"
string(3) "inf"
string(3) "inf"
string(3) "inf"
string(3) "nan"
string(3) "nan"
int(102)
string(1) "1"
string(1) "2"
string(1) "3"
string(2) "-2"
string(8) "1,234.57"
string(8) "1.234,57"
string(6) "123457"
string(15) "1__234__567<>89"
string(8) "1,234.57"
string(4) "0.05"
string(12) "1,000,000.00"
string(2) "40"
string(5) "1,000"
0 float $num
1 int $decimals
2 ?string $decimal_separator
3 ?string $thousands_separator
number_format(): Argument #1 ($num) must be of type int|float, array given
number_format(): Argument #1 ($num) must be of type int|float, string given
number_format(): Argument #1 ($num) must be of type int|float, string given
number_format(): Argument #2 ($decimals) must be of type int, string given
number_format(): Argument #3 ($decimal_separator) must be of type ?string, array given
number_format() expects at least 1 argument, 0 given
number_format() expects at most 4 arguments, 5 given
--CLEAN--
<?php
