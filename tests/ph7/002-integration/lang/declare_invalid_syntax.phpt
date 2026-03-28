--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Declare statement with invalid syntax
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
declare invalid;
echo "should not reach here\n";
?>
--EXPECTF--
%s Error:  declare: Expecting opening parenthesis '(' %s
--CLEAN--
<?php

