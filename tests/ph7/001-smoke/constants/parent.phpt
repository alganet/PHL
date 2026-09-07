--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
`parent` is a keyword, not a constant: using it as a bare word is an Error
--FILE--
<?php
// Caught, not uncaught: the smoke tier shares one interpreter, so an escaping fatal
// would bail the whole run.
try {
    echo parent;
    echo "FAIL: parent expanded to a value";
} catch (Error $e) {
    echo $e->getMessage();
}
?>
--EXPECT--
Undefined constant "parent"
--CLEAN--
<?php
