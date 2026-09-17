--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
assert("") throws AssertionError (empty string is falsy)
--INI--
zend.assertions=1
--SKIPIF--
<?php
// php renders the assertion's SOURCE TEXT in the AssertionError -- assert(null),
// assert(0), assert(''), assert([]) -- because it records the argument's source
// span at compile time. PHL always reports assert(false), having only the
// evaluated value. A recorded divergence; this test pins PHL's current message,
// and retires with that feature.
if (function_exists('zend_version')) { echo 'skip php echoes the assertion source text; PHL reports the evaluated value'; }
?>
--FILE--
<?php
assert("");
?>
--EXPECTF--
%s Fatal error:  Uncaught AssertionError: assert(false) in %s
--CLEAN--
<?php

