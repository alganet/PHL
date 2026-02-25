--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
addcslashes(NULL mask) should raise TypeError (PHL-only behaviour)
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
addcslashes('abc', null);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: addcslashes(): Argument #2 ($characters) must be of type string, %s given in %s
--CLEAN--
<?php


