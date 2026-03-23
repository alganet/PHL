--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
base_convert object input throws TypeError
--SKIPIF--
<?php if (!function_exists('zend_version')) echo 'skip PHL reports object instead of class name'; ?>
--FILE--
<?php
base_convert(new stdClass, 10, 16);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: base_convert(): Argument #1 ($num) must be of type string, stdClass given in %s
--CLEAN--
<?php

