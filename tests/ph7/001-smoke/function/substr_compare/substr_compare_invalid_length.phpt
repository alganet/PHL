--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
substr_compare() rejects a negative length with php's ValueError
--FILE--
<?php
// Caught, not uncaught: the smoke tests share one interpreter, so an escaping fatal
// would bail the entire run.
try {
    substr_compare('a', 'b', 0, -1);
    echo "FAIL: no error";
} catch (ValueError $e) {
    echo $e->getMessage();
}
?>
--EXPECT--
substr_compare(): Argument #4 ($length) must be greater than or equal to 0
--CLEAN--
<?php
