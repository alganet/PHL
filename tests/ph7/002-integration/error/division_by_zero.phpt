--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Division by zero in expressions
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$result = 10 / 0;
echo "Result: $result\n";
?>
--EXPECTF--
%s Error: Division by zero
Result: 0
--CLEAN--
<?php
unset($result);
