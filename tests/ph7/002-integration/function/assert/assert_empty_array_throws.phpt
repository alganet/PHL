--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(array()) throws AssertionError naming the source ([] like php's export)
--INI--
zend.assertions=1
--FILE--
<?php
assert(array());
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: assert([]) in %s
--CLEAN--
<?php
