--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Logical operators 'and', 'or', 'xor' processing
--FILE--
<?php
// Test logical operators 'and', 'or', 'xor'
// Covers operator lookup for alpha operators (parse.c ~261)

$a = true;
$b = false;

echo ($a and $b) ? 'true' : 'false';
echo "\n";
echo ($a or $b) ? 'true' : 'false';
echo "\n";
echo ($a xor $b) ? 'true' : 'false';
echo "\n";
echo ($a and true) ? 'true' : 'false';
?>
--EXPECT--
false
true
true
true
--CLEAN--
<?php
unset($a, $b);
?>