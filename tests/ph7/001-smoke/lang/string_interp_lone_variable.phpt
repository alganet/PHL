--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A double-quoted string that is nothing but one interpolation still produces a string
--FILE--
<?php
// With no concatenation to force the conversion, "$x" used to hand back $x's own
// value untouched, so gettype() reported the ORIGINAL type here.
$int = 42;
$float = 1.5;
$bool = true;
$null = null;
$str = 'hi';

foreach (['int' => $int, 'float' => $float, 'bool' => $bool, 'null' => $null, 'str' => $str] as $name => $value) {
    $out = "$value";
    echo str_pad($name, 6), gettype($out), ' ', var_export($out, true), "\n";
}

// Mixed and literal forms were always strings; check they still are.
echo 'mixed ', gettype("x$int"), ' ', var_export("x$int", true), "\n";
echo 'brace ', gettype("{$int}"), ' ', var_export("{$int}", true), "\n";
echo 'plain ', gettype("abc"), "\n";
?>
--EXPECT--
int   string '42'
float string '1.5'
bool  string '1'
null  string ''
str   string 'hi'
mixed string 'x42'
brace string '42'
plain string
--CLEAN--
<?php
unset($int, $float, $bool, $null, $str, $out);
