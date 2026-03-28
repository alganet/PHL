--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace qualification must not corrupt interned string literals
--FILE--
<?php
namespace App;

function greet() { return "hello"; }

// "greet" used as a string value
$name = "greet";

// Function call triggers NS qualification of identifier
echo greet(), "\n";

// The string variable must not be corrupted
echo $name, "\n";

// Repeated function calls must still work
echo greet(), "\n";
echo greet(), "\n";
?>
--EXPECT--
hello
greet
hello
hello
--CLEAN--
<?php
