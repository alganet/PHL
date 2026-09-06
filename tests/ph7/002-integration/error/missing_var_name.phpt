--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: missing variable name
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = ${};
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected token "}", expecting variable or "{" or "$"%A
--CLEAN--
<?php
unset($a);
