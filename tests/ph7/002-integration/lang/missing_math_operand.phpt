--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
PHL: invalid expression syntax triggers parse error
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
$a = 1 + ;
?>
--EXPECTF--
%s Error:  '+': Missing/Invalid operand %s
--CLEAN--
<?php
unset($a);
