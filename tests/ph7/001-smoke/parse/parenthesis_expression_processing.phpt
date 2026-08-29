--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Test parenthesis expression processing

--FILE--
<?php
// Test expressions with nested parentheses to cover ExprMakeTree recursion
$result = ((1 + 2) * (3 + 4));
var_dump($result); // Should be 21

$result2 = (((5)));
var_dump($result2); // Should be 5

$result3 = (1) + (2) * (3);
var_dump($result3); // Should be 7
?>
--EXPECT--
int(21)
int(5)
int(7)
--CLEAN--
<?php
unset($result, $result2, $result3);
