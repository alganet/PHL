--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mixed return type accepts any explicitly returned value, including null
--FILE--
<?php
class MixedRetBox {}
function mixedRetArray(): mixed { return [1, 2]; }
function mixedRetString(): mixed { return "x"; }
function mixedRetInt(): mixed { return 7; }
function mixedRetNull(): mixed { return null; }
function mixedRetObject(): mixed { return new MixedRetBox(); }

echo implode(",", mixedRetArray()), "\n";
echo mixedRetString(), "\n";
echo mixedRetInt(), "\n";
echo mixedRetNull() === null ? "null\n" : "notnull\n";
echo mixedRetObject() instanceof MixedRetBox ? "obj\n" : "notobj\n";
?>
--EXPECT--
1,2
x
7
null
obj
--CLEAN--
<?php
