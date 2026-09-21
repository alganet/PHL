--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert("") throws AssertionError naming the source
--INI--
zend.assertions=1
--FILE--
<?php
assert("");
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: assert('') in %s
--CLEAN--
<?php
