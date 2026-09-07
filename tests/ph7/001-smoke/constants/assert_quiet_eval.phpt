--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ASSERT_QUIET_EVAL was removed in php 8.0: referencing it is an Error
--FILE--
<?php
// Caught, not uncaught: the smoke tier shares one interpreter, so an escaping fatal
// would bail the whole run.
try {
    echo ASSERT_QUIET_EVAL;
    echo "FAIL: still defined";
} catch (Error $e) {
    echo $e->getMessage();
}
?>
--EXPECT--
Undefined constant "ASSERT_QUIET_EVAL"
--CLEAN--
<?php
