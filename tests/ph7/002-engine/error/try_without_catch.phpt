--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
try without catch
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
try {
echo "hello";
}
--EXPECTF--
%s 4 Error: Try: Unexpected token '}',expecting 'catch' block
Compile error
