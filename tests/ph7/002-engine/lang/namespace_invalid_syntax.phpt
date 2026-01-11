--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace with invalid token after keyword
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
namespace 123;
echo "should not reach here\n";
?>
--EXPECTF--
%s 2 Error: Namespace: Unexpected token '123'