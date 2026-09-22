--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
number_format() enforces php's four ZPP rows instead of casting whatever it is handed
--DESCRIPTION--
number_format() is a PRELUDE function (embedded PHP), so it is out of reach of the
aBuiltinSig[] ZPP screen, which only stamps HOST builtins. Nothing checked its arguments: an
array, a not-stringable object, a resource and a non-numeric string all reached the `(float)`
cast and ANSWERED -- `number_format(new Bare())` warned and returned "1", `number_format("abc")`
returned "0", and an object separator produced "Array"/an Error from the concatenation instead
of a ZPP TypeError. The four rows are checked in the body now, in the shape php's ZPP reports
them, through the same `__php_zpp_type` helper count_chars()/max()/min() use.

The two §10 policy divergences (null and a lossy float, which php deprecates and coerces) are
in the number_format_argument_types_policy{,_zend}.phpt twin pair, not here.
--FILE--
<?php
class NfBare {}
class NfStr { public function __toString() { return "!"; } }

function t($label, $fn) {
    echo "$label: ";
    try { var_dump($fn()); } catch (Throwable $e) { echo get_class($e), ": ", $e->getMessage(), "\n"; }
}

echo "== \$num must be int|float\n";
t('object',           fn() => number_format(new NfBare()));
t('stringable object', fn() => number_format(new NfStr()));
t('array',            fn() => number_format([1]));
t('non-numeric str',  fn() => number_format("abc"));
t('leading-num str',  fn() => number_format("12abc"));
t('hex-looking str',  fn() => number_format("0x1A"));
t('resource',         function () { $f = fopen('php://memory', 'r'); return number_format($f); });

echo "== ...and accepts what php accepts\n";
t('int',              fn() => number_format(1234567));
t('float',            fn() => number_format(1234.567, 2));
t('numeric string',   fn() => number_format("1234.5", 1));
t('padded numeric',   fn() => number_format(" 12 "));
t('exponent string',  fn() => number_format("1e3"));
t('true',             fn() => number_format(true));
t('false',            fn() => number_format(false));

echo "== \$decimals must be int\n";
t('object',           fn() => number_format(1.5, new NfBare()));
t('array',            fn() => number_format(1.5, [1]));
t('non-numeric str',  fn() => number_format(1.5, "2abc"));
t('resource',         function () { $f = fopen('php://memory', 'r'); return number_format(1.5, $f); });
t('numeric string',   fn() => number_format(1234.5, "2"));
t('whole float',      fn() => number_format(1234.5, 2.0));
t('bool',             fn() => number_format(1234.5, true));
t('negative',         fn() => number_format(1234.5, -1));

echo "== the separators are ?string\n";
t('#3 object',        fn() => number_format(1234.5, 2, new NfBare()));
t('#3 array',         fn() => number_format(1234.5, 2, ['.']));
t('#4 object',        fn() => number_format(1234.5, 2, '.', new NfBare()));
t('#4 array',         fn() => number_format(1234.5, 2, '.', [',']));
t('#3 resource',      function () { $f = fopen('php://memory', 'r'); return number_format(1234.5, 2, $f); });
t('#3/#4 null',       fn() => number_format(1234.5, 2, null, null));
t('#3/#4 stringable', fn() => number_format(1234.5, 2, new NfStr(), new NfStr()));
t('#3/#4 int',        fn() => number_format(1234.5, 2, 5, 6));
t('#3/#4 multichar',  fn() => number_format(1234.5, 2, "::", "||"));
t('#3/#4 empty',      fn() => number_format(1234.5, 2, "", ""));
?>
--EXPECT--
== $num must be int|float
object: TypeError: number_format(): Argument #1 ($num) must be of type int|float, NfBare given
stringable object: TypeError: number_format(): Argument #1 ($num) must be of type int|float, NfStr given
array: TypeError: number_format(): Argument #1 ($num) must be of type int|float, array given
non-numeric str: TypeError: number_format(): Argument #1 ($num) must be of type int|float, string given
leading-num str: TypeError: number_format(): Argument #1 ($num) must be of type int|float, string given
hex-looking str: TypeError: number_format(): Argument #1 ($num) must be of type int|float, string given
resource: TypeError: number_format(): Argument #1 ($num) must be of type int|float, resource given
== ...and accepts what php accepts
int: string(9) "1,234,567"
float: string(8) "1,234.57"
numeric string: string(7) "1,234.5"
padded numeric: string(2) "12"
exponent string: string(5) "1,000"
true: string(1) "1"
false: string(1) "0"
== $decimals must be int
object: TypeError: number_format(): Argument #2 ($decimals) must be of type int, NfBare given
array: TypeError: number_format(): Argument #2 ($decimals) must be of type int, array given
non-numeric str: TypeError: number_format(): Argument #2 ($decimals) must be of type int, string given
resource: TypeError: number_format(): Argument #2 ($decimals) must be of type int, resource given
numeric string: string(8) "1,234.50"
whole float: string(8) "1,234.50"
bool: string(7) "1,234.5"
negative: string(5) "1,230"
== the separators are ?string
#3 object: TypeError: number_format(): Argument #3 ($decimal_separator) must be of type ?string, NfBare given
#3 array: TypeError: number_format(): Argument #3 ($decimal_separator) must be of type ?string, array given
#4 object: TypeError: number_format(): Argument #4 ($thousands_separator) must be of type ?string, NfBare given
#4 array: TypeError: number_format(): Argument #4 ($thousands_separator) must be of type ?string, array given
#3 resource: TypeError: number_format(): Argument #3 ($decimal_separator) must be of type ?string, resource given
#3/#4 null: string(8) "1,234.50"
#3/#4 stringable: string(8) "1!234!50"
#3/#4 int: string(8) "16234550"
#3/#4 multichar: string(10) "1||234::50"
#3/#4 empty: string(6) "123450"
