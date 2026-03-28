--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unexpected closing brace
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo 1;
}
?>
--EXPECTF--
%s Error:  Syntax error: Unexpected token '}' %s
--CLEAN--
<?php

