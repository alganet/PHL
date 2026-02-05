--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
List syntax error: missing closing parenthesis ')'
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = list(1,2 ;
?>
--EXPECTF--
%s 2 Error: list: Missing closing parenthesis ')'
Compile error
--CLEAN--
<?php
unset($a);
