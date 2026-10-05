--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: a whole-valued float literal is a float, refused for an int
--FILE--
<?php
echo "never printed\n";
class TpdcrHolder {
    public static int $x = -2.0;
}
--EXPECTF--
PHP Fatal error:  Cannot use float as default value for property TpdcrHolder::$x of type int in %s on line 4%A
--CLEAN--
<?php
