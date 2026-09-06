--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
const with reserved names
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
const null = 1;
const true = 2;
const false = 3;
echo "Should not reach here\n";
?>
--EXPECTF--
%AFatal error:%ACannot redeclare constant 'null'%AFatal error:%ACannot redeclare constant 'true'%AFatal error:%ACannot redeclare constant 'false'%A
--CLEAN--
<?php

