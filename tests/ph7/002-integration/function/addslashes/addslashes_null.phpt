--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addslashes(NULL) should raise TypeError (we treat PHP deprecation as full error)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
addslashes(NULL);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: addslashes(): Argument #1 ($string) must be of type string, %s given in %s

--CLEAN--
<?php

