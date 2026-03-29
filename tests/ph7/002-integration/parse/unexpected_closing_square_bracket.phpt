--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
unexpected closing square bracket
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
echo $a];
?>
--EXPECTF--
%s Fatal error:  Syntax error: Unexpected token ']' %s
--CLEAN--
<?php
unset($a);
