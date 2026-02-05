--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Invalid octal escape sequences in double quoted strings
--FILE--
<?php
// Test invalid octal escape \oa (should output 'oa')
$s = "\oa";
echo $s . "\n";
// Test \o8 (8 is invalid octal digit >7)
$s = "\o8";
echo $s . "\n";
?>
--EXPECT--
oa
o8
--CLEAN--
<?php
unset($s);
