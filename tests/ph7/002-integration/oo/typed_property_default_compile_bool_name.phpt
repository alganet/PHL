--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed property: a refused bool default is named true or false, on the line its declaration statement starts
--FILE--
<?php
echo "never printed\n";
class TpdcbHolder {
    public int $a = 1,
        $b = 2 > 1;
}
--EXPECTF--
PHP Fatal error:  Cannot use true as default value for property TpdcbHolder::$b of type int in %s on line 4%A
--CLEAN--
<?php
