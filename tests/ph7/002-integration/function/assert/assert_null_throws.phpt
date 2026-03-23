--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert(null) throws AssertionError
--SKIPIF--
<?php if (function_exists('zend_version')) { echo 'skip'; } ?>
--FILE--
<?php
assert(null);
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: assert(false) in %s
--CLEAN--
<?php

