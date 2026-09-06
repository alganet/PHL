--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: php 8.x diagnostics — string ++/--, array_sum, base conversion
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip deprecations not ready yet'; ?>
--FILE--
<?php
// php 8.3: ++ and -- on a non-numeric string both deprecate (different texts),
// and the VALUES are unchanged: -- is a no-op, ++ is Perl-style.
$s = 'abc';
$s--;
echo var_export($s, true), "\n";
$t = 'abc';
$t++;
echo var_export($t, true), "\n";
// numeric strings are untouched and silent
$n = '5';
$n++;
echo var_export($n, true), "\n";

// array_sum warns once per element it cannot add, and skips that element
echo var_export(array_sum([1, 'x', 2]), true), "\n";
echo var_export(array_sum([1, '', 3]), true), "\n";
echo var_export(array_sum([1, [2]]), true), "\n";
// the warning names the CLASS of an object, not the word "object"
echo var_export(array_sum([1, new stdClass()]), true), "\n";
// the float path warns identically
echo var_export(array_sum([1.5, '', 3.5]), true), "\n";
// numeric strings, bool and null add silently
echo var_export(array_sum([1, '2', true, null]), true), "\n";

// base conversion deprecates ignored characters; the value is unaffected
echo var_export(hexdec('1z2'), true), "\n";
echo var_export(hexdec('ff'), true), "\n";
?>
--EXPECTF--
%ADecrement on non-numeric string has no effect and is deprecated%A
'abc'
%AIncrement on non-numeric string is deprecated, use str_increment() instead%A
'abd'
6
%Aarray_sum(): Addition is not supported on type string%A
3
%Aarray_sum(): Addition is not supported on type string%A
4
%Aarray_sum(): Addition is not supported on type array%A
1
%Aarray_sum(): Addition is not supported on type stdClass%A
1
%Aarray_sum(): Addition is not supported on type string%A
5.0
4
%AInvalid characters passed for attempted conversion, these have been ignored%A
18
255
--CLEAN--
<?php
unset($s, $t, $n);
