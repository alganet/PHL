--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(false, "msg") throws AssertionError with custom message
--INI--
zend.assertions=1
--FILE--
<?php
assert(false, "custom message");
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: custom message in %s
--CLEAN--
<?php

