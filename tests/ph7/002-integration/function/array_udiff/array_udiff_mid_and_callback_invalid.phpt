--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--SKIPIF--
<?php if (function_exists('zend_version')) echo 'skip';
?>
--TEST--
When an intermediate argument is not an array and the callback is invalid, the intermediate argument error should be thrown first
--FILE--
<?php
array_udiff(array(1), "not an array", 123);
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_udiff(): Argument #2 must be of type array, string given in %s
--CLEAN--
<?php

