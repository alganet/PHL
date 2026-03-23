--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert() with no arguments throws ArgumentCountError
--SKIPIF--
<?php if (function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
assert();
?>
--EXPECTF--
%s Fatal error:  Uncaught ArgumentCountError: assert() expects at least 1 argument, 0 given in %s
--CLEAN--
<?php

