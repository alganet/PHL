--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A static PROPERTY fetch in a constant expression is "invalid operations", not the static:: sentence
--FILE--
<?php
class CesC {
    public static $q = 1;
    const C = static::$q;
}
echo "unreachable\n";
?>
--EXPECTF--
%AConstant expression contains invalid operations%A
--CLEAN--
<?php
