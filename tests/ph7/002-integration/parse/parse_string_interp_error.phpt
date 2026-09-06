--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
String interpolation syntax error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = "${1+";
echo "test";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected variable "$"%A
--CLEAN--
<?php
unset($a);
