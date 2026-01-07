--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Invalid hex literal
--FILE--
<?php
$a = 0xG;
echo "Should not reach here\n";
?>
--EXPECTF--
%s 2 Error: Unexpected token 'G'
Compile error
