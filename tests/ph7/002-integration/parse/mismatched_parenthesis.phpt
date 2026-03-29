--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
mismatched parenthesis
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo (1 + 2;
?>
--EXPECTF--
%s Fatal error:  Syntax error,mismatched '(','[','{' or '?' %s
--CLEAN--
<?php

