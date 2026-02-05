--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
invalid function and const names
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
function 123() {}
const 456 = 789;
echo "Should not reach here\n";
?>
--EXPECTF--
%s 2 Error: Invalid function name
%s 3 Error: const: Invalid constant name
Compile error
--CLEAN--
<?php

