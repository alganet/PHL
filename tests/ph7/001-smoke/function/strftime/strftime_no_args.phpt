--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
strftime() with no arguments throws ArgumentCountError (after the 8.1 deprecation)
--FILE--
<?php
// the whole-function deprecation fires FIRST, then ZPP rejects the call
set_error_handler(function () { return true; });
try {
    strftime();
} catch (ArgumentCountError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
strftime() expects at least 1 argument, 0 given
--CLEAN--
<?php
