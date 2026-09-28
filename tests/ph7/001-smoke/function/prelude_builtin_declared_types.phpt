--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A builtin written as prelude PHP declares php's own parameter and return types
--DESCRIPTION--
Twenty-two builtins are implemented as embedded PHP in the prelude, and every
one of them declared its parameters UNTYPED where php declares a type. The
declaration is the ZPP screen, not decoration: is_nan('abc') answered false
where php raises a TypeError, array_count_values('x') walked a string, and each
body opened with a manual cast that was hiding exactly that. Their return types
were missing too, so ReflectionFunction printed none.
--FILE--
<?php
function pbdtShow($label, $fn) {
    try { $out = var_export($fn(), true); }
    catch (Throwable $e) { $out = get_class($e) . ': ' . $e->getMessage(); }
    echo $label, ' => ', str_replace("\n", '', $out), "\n";
}

/* Every one of these used to be CAST and answered. */
pbdtShow('is_nan',              fn() => is_nan('abc'));
pbdtShow('is_infinite',         fn() => is_infinite('abc'));
pbdtShow('is_finite',           fn() => is_finite([1]));
pbdtShow('hex2bin',             fn() => hex2bin([1]));
pbdtShow('checkdate',           fn() => checkdate('a', 1, 2020));
pbdtShow('doubleval',           fn() => doubleval('12'));
pbdtShow('array_count_values',  fn() => array_count_values('x'));
pbdtShow('array_change_key_case', fn() => array_change_key_case('x'));
pbdtShow('array_replace_recursive', fn() => array_replace_recursive([1], 'x'));
pbdtShow('ip2long',             fn() => ip2long([1]));
pbdtShow('long2ip',             fn() => long2ip('abc'));
pbdtShow('preg_grep',           fn() => preg_grep('/a/', 'notanarray'));
pbdtShow('preg_filter',         fn() => preg_filter(new stdClass, 'x', 'y'));
pbdtShow('preg_replace_callback_array', fn() => preg_replace_callback_array('x', 'y'));
pbdtShow('str_increment',       fn() => str_increment([1]));
pbdtShow('str_decrement',       fn() => str_decrement([1]));
pbdtShow('clearstatcache',      fn() => clearstatcache(false, []));

/* The screen COERCES what php coerces -- these still answer. */
pbdtShow('is_nan(numeric str)', fn() => is_nan('12'));
pbdtShow('checkdate(str)',      fn() => checkdate('2', '29', '2024'));
pbdtShow('long2ip(str)',        fn() => long2ip('16909060'));
pbdtShow('ip2long(ok)',         fn() => ip2long('1.2.3.4'));

/* php's return types, which Reflection prints. */
foreach (['is_nan', 'hex2bin', 'checkdate', 'doubleval', 'array_count_values',
          'array_change_key_case', 'array_replace_recursive', 'ip2long', 'long2ip',
          'preg_grep', 'preg_filter', 'preg_replace_callback_array', 'str_increment',
          'str_decrement', 'clearstatcache', 'scandir', 'glob', 'tempnam',
          'class_parents', 'class_implements', 'class_uses'] as $pbdtName) {
    $pbdtRef = new ReflectionFunction($pbdtName);
    echo $pbdtName, ' : ', (string)$pbdtRef->getReturnType(), "\n";
}

/* And the parameter lines. */
foreach (['checkdate', 'preg_grep', 'array_change_key_case'] as $pbdtName) {
    foreach ((new ReflectionFunction($pbdtName))->getParameters() as $pbdtParam) {
        echo $pbdtName, ' #', $pbdtParam->getPosition(), ' ',
             (string)$pbdtParam->getType(), ' $', $pbdtParam->getName(), "\n";
    }
}
--EXPECT--
is_nan => TypeError: is_nan(): Argument #1 ($num) must be of type float, string given
is_infinite => TypeError: is_infinite(): Argument #1 ($num) must be of type float, string given
is_finite => TypeError: is_finite(): Argument #1 ($num) must be of type float, array given
hex2bin => TypeError: hex2bin(): Argument #1 ($string) must be of type string, array given
checkdate => TypeError: checkdate(): Argument #1 ($month) must be of type int, string given
doubleval => 12.0
array_count_values => TypeError: array_count_values(): Argument #1 ($array) must be of type array, string given
array_change_key_case => TypeError: array_change_key_case(): Argument #1 ($array) must be of type array, string given
array_replace_recursive => TypeError: array_replace_recursive(): Argument #2 must be of type array, string given
ip2long => TypeError: ip2long(): Argument #1 ($ip) must be of type string, array given
long2ip => TypeError: long2ip(): Argument #1 ($ip) must be of type int, string given
preg_grep => TypeError: preg_grep(): Argument #2 ($array) must be of type array, string given
preg_filter => TypeError: preg_filter(): Argument #1 ($pattern) must be of type array|string, stdClass given
preg_replace_callback_array => TypeError: preg_replace_callback_array(): Argument #1 ($pattern) must be of type array, string given
str_increment => TypeError: str_increment(): Argument #1 ($string) must be of type string, array given
str_decrement => TypeError: str_decrement(): Argument #1 ($string) must be of type string, array given
clearstatcache => TypeError: clearstatcache(): Argument #2 ($filename) must be of type string, array given
is_nan(numeric str) => false
checkdate(str) => true
long2ip(str) => '1.2.3.4'
ip2long(ok) => 16909060
is_nan : bool
hex2bin : string|false
checkdate : bool
doubleval : float
array_count_values : array
array_change_key_case : array
array_replace_recursive : array
ip2long : int|false
long2ip : string
preg_grep : array|false
preg_filter : array|string|null
preg_replace_callback_array : array|string|null
str_increment : string
str_decrement : string
clearstatcache : void
scandir : array|false
glob : array|false
tempnam : string|false
class_parents : array|false
class_implements : array|false
class_uses : array|false
checkdate #0 int $month
checkdate #1 int $day
checkdate #2 int $year
preg_grep #0 string $pattern
preg_grep #1 array $array
preg_grep #2 int $flags
array_change_key_case #0 array $array
array_change_key_case #1 int $case
