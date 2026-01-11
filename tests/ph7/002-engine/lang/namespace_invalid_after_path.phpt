--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace with invalid token after path
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
namespace my\ns 123;
echo "should not reach here\n";
?>
--EXPECTF--
%s 2 Error: Namespace: Unexpected token '123',expecting ';' or '{'