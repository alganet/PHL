--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
unexpected closing square bracket
--FILE--
<?php
$a = [1];
echo $a];
?>
--EXPECTF--
%s 2 Error: Invalid array name
%s 3 Error: Syntax error: Unexpected token ']'
Compile error
--CLEAN--
<?php
unset($a);
