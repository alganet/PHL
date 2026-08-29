--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation with complex curly brace syntax

--FILE--
<?php
$name = 'world';
$arr = array('key' => 'value', 'nested' => array('deep' => 'found'));

// Simple curly brace syntax
echo "Hello {$name}!\n";

// Array access with curly braces
echo "Got: {$arr['key']}\n";

// Nested array access
echo "Deep: {$arr['nested']['deep']}\n";

// Expression in curly braces
$a = 2;
$b = 3;
echo "Sum: {$a} + {$b}\n";

// Variable variable in curly braces
$varname = 'name';
echo "Dynamic: {$$varname}\n";
?>
--EXPECT--
Hello world!
Got: value
Deep: found
Sum: 2 + 3
Dynamic: world
--CLEAN--
<?php
unset($name, $arr, $a, $b, $varname);
