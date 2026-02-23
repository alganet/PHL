--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Empty single-quoted string literal
--FILE--
<?php
$a = '';
echo "Type: " . gettype($a) . "\n";
echo "Length: " . strlen($a) . "\n";
echo "Value: '" . $a . "'\n";
?>
--EXPECT--
Type: string
Length: 0
Value: ''
--CLEAN--
<?php
unset($a);
