--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(false, "msg") throws AssertionError with custom message
--SKIPIF--
<?php if (function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
assert(false, "custom message");
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: custom message in %s
--CLEAN--
<?php

