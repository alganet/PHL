--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid hexadecimal escape sequences in double-quoted strings
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test invalid \x escape sequences
// Covers GenStateCompileString invalid hex handling

$invalid1 = "\xG";
$invalid2 = "\x";
$invalid3 = "\xZZ";

echo "Invalid hex \\xG: '$invalid1'\n";
echo "Incomplete \\x: '$invalid2'\n";
echo "Invalid hex \\xZZ: '$invalid3'\n";

// Valid for comparison
$valid = "\x41";
echo "Valid \\x41: '$valid'\n";

echo "Done\n";
?>
--EXPECT--
Invalid hex \xG: 'xG'
Incomplete \x: 'x'
Invalid hex \xZZ: 'xZZ'
Valid \x41: 'A'
Done