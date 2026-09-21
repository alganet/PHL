--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(0) throws AssertionError naming the source
--INI--
zend.assertions=1
--FILE--
<?php
assert(0);
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: assert(0) in %s
--CLEAN--
<?php
