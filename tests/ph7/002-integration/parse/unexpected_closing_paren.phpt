--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
unexpected closing parenthesis
--FILE--
<?php
echo 1);
?>
--EXPECTF--
%s 2 Error: Syntax error: Unexpected token ')'
Compile error
--CLEAN--
<?php

