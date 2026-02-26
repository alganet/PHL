--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_fill: boolean count raises TypeError (PHL-only)
--SKIPIF--
<?php if(function_exists('zend_version')) echo 'skip'; ?>
--FILE--
<?php
array_fill(0, true, 'x');
?>
--EXPECTF--
PHP Fatal error:  Uncaught TypeError: array_fill(): Argument #2 ($count) must be of type int, bool given in %s
--CLEAN--
<?php

