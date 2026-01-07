--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String escape edge cases
--FILE--
<?php
// Test various string escape sequences
$hex = "\x41\x42\x43";   // Hex escapes

echo $hex . "\n";
?>
--EXPECT--
ABC
--CLEAN--
<?php
unset($hex);
?>