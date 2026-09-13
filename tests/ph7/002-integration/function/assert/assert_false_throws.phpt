--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(false) throws AssertionError
--INI--
zend.assertions=1
--FILE--
<?php
assert(false);
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: assert(false) in %s
--CLEAN--
<?php

