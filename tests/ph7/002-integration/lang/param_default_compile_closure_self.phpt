--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Typed parameter: a closure keeps self as written in the refusal, its scope can be rebound
--FILE--
<?php
echo "never printed\n";
class PdccsHolder {
    function m() { return function (self|int $p = "x") {}; }
}
--EXPECTF--
PHP Fatal error:  Cannot use string as default value for parameter $p of type self|int in %s on line 4%A
--CLEAN--
<?php
