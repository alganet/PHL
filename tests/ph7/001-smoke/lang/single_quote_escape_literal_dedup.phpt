--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PH7 / PHP: an escaped single-quoted literal is not aliased by the literal cache
--FILE--
<?php
// The compiler dedups string literals, but keyed FIND on the raw source and
// INSTALL on the unescaped value. For escaped literals those differ, so '\\'
// (raw \\, value \) could wrongly reuse the entry installed for '\\\\' (raw
// \\\\, value \\) and load two backslashes. Both orders must stay distinct.
$sqdTwo = '\\\\';
$sqdOne = '\\';
echo strlen($sqdTwo), strlen($sqdOne), "\n";
echo bin2hex($sqdTwo), " ", bin2hex($sqdOne), "\n";
echo ($sqdOne === $sqdTwo ? "SAME" : "DIFF"), "\n";
// reverse declaration order (single before double)
$sqdOneR = '\\';
$sqdTwoR = '\\\\';
echo strlen($sqdOneR), strlen($sqdTwoR), "\n";
// escaped single quote vs backslash-then-quote
$sqdQ  = '\'';
$sqdBq = '\\\'';
echo bin2hex($sqdQ), " ", bin2hex($sqdBq), "\n";
// a backslash mid-literal must not collide with a namespace-shaped literal
$sqdNs  = 'Foo\\Bar';
$sqdNs2 = 'Foo\\\\Bar';
echo strlen($sqdNs), strlen($sqdNs2), "\n";
// the regex round-trip that first exposed the bug
$sqdRe = '/^' . preg_quote('\\', '/') . '$/s';
var_export(preg_match($sqdRe, '\\'));   echo "\n";
var_export(preg_match($sqdRe, '\\\\')); echo "\n";
?>
--EXPECT--
21
5c5c 5c
DIFF
12
27 5c27
78
1
0
