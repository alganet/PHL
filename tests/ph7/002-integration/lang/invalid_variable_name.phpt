--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--TEST--
Invalid variable name
--FILE--
<?php
$a = $1;
echo "Should not reach here\n";
?>
--EXPECTF--
%s 2 Error: Unexpected token '1'
Compile error
--CLEAN--
<?php
unset($a);
