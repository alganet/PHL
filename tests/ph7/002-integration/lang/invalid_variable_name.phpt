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
%s Error:  Unexpected token '1' %s
--CLEAN--
<?php
unset($a);
