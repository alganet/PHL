--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
realpath with invalid arguments
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// No arguments
$result = realpath();
if ($result === false) echo "PASS_NO_ARGS\n"; else echo "FAIL_NO_ARGS\n";

// Non-string argument
$result = realpath(123);
if ($result === false) echo "PASS_NON_STRING\n"; else echo "FAIL_NON_STRING\n";
?>
--EXPECT--
PASS_NO_ARGS
PASS_NON_STRING