--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Return type declarations on closures and case-insensitive void
--FILE--
<?php
$getInt = function(): int { return 42; };
echo $getInt() . "\n";

$greet = function(string $name): string { return "Hello $name"; };
echo $greet("World") . "\n";

function doVoid(): Void { }
doVoid();
echo "void ok\n";
?>
--EXPECT--
42
Hello World
void ok
--CLEAN--
<?php

