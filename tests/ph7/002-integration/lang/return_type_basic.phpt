--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Return type declarations for scalar types, array, and void
--FILE--
<?php
function getInt(): int { return 42; }
function getString(): string { return "hello"; }
function getBool(): bool { return true; }
function getFloat(): float { return 3.14; }
function getArray(): array { return [1, 2, 3]; }
function doVoid(): void { }

echo getInt() . "\n";
echo getString() . "\n";
echo getBool() . "\n";
echo getFloat() . "\n";
echo count(getArray()) . "\n";
doVoid();
echo "void ok\n";
?>
--EXPECT--
42
hello
1
3.14
3
void ok
--CLEAN--
<?php

