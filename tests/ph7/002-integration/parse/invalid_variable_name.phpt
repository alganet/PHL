--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid variable name compilation error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test invalid variable name to cover line 1440 in compile.c
$1 = 5;
?>
--EXPECTF--
%s Fatal error:  '=': Left operand must be a modifiable l-value %s
--CLEAN--
<?php
unset($1);
