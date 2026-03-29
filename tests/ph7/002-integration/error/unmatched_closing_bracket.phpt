--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Unmatched closing bracket should produce syntax error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
// Test unmatched closing bracket
echo "test";
]
?>
--EXPECTF--
%s Fatal error:  Syntax error: Unexpected token ']' %s
--CLEAN--
<?php

