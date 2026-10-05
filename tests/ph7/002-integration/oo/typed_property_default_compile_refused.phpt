--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: a literal default of the wrong type stops the file at compile time, even in a class nothing declares
--FILE--
<?php
// php folds a typed property's default when it compiles the class and holds the
// value to the declared type then -- not when the class is instantiated, and not
// only when the declaration runs. Nothing below may print.
echo "never printed\n";
if (false) {
    class TpdcHolder {
        public string $ok = "a" . "b";
        public int $bad = "a";
    }
}
--EXPECTF--
PHP Fatal error:  Cannot use string as default value for property TpdcHolder::$bad of type int in %s on line 9%A
--CLEAN--
<?php
