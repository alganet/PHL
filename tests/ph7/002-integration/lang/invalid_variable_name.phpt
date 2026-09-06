--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid variable name
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = $1;
echo "Should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected integer "1"%A
--CLEAN--
<?php
unset($a);
