--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Invalid hex literal
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = 0xG;
echo "Should not reach here\n";
?>
--EXPECTF--
%AParse error:%Asyntax error, unexpected identifier "G"%A
--CLEAN--
<?php
unset($a);
