--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array() with extended invalid reference expressions
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = array(&$b++);
$a = array(&$c->d);
$a = array(&$e + $f);
echo "Should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "++", expecting "->" or "?->" or "["%AParse error:%Asyntax error, unexpected token "+", expecting "->" or "?->" or "["%A
--CLEAN--
<?php
unset($a);
