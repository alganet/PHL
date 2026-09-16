--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A constant is a valid subscript base: X[0] used to raise "Invalid array name" while CSubBase::X[0] and the via-variable detour worked
--FILE--
<?php
// Unique names: smoke tests share one interpreter, so top-level constants and
// class names must not collide with another test's (tests/phptrunner/README.md).
const CSUB_ARR = [1, 2, 3];
const CSUB_NESTED = [1, [2, 3]];
const CSUB_TXT = 'abc';
define('CSUB_DEFINED', [9, 8]);

class CSubBase { const X = [4, 5]; }

echo 'const:    ', CSUB_ARR[0], CSUB_ARR[2], "\n";
echo 'nested:   ', CSUB_NESTED[1][0], "\n";
echo 'string:   ', CSUB_TXT[1], "\n";
echo 'define:   ', CSUB_DEFINED[1], "\n";
echo 'class:    ', CSubBase::X[0], "\n";
echo 'variable: ', (function () { $a = CSUB_ARR; return $a[1]; })(), "\n";

// The other subscript bases php accepts, which already worked.
echo 'literal:  ', [10, 20][1], "\n";
echo 'strlit:   ', "xyz"[2], "\n";
echo 'call:     ', (function () { return [7, 8]; })()[0], "\n";
?>
--EXPECT--
const:    13
nested:   2
string:   b
define:   8
class:    4
variable: 2
literal:  20
strlit:   z
call:     7
--CLEAN--
<?php
