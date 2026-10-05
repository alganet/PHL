--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: a folded default of the wrong type stops the file at compile time, even in a function nothing declares
--FILE--
<?php
// php folds a typed parameter's default when it compiles the function and holds
// the value to the declared type then -- not when the function is called, and
// not only when the declaration runs. Nothing below may print.
echo "never printed\n";
if (false) {
    function pdcr_ok(string $s = "a" . "b") {}
    function pdcr_bad(int $x = "a" . "b") {}
}
--EXPECTF--
PHP Fatal error:  Cannot use string as default value for parameter $x of type int in %s on line 8%A
--CLEAN--
<?php
