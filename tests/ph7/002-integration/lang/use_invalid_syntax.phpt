--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Use statement with invalid syntax
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
use 123;
echo "should not reach here\n";
?>
--EXPECTF--
%s 2 Error: use statement: Unexpected token '123',expecting ';'
--CLEAN--
<?php

