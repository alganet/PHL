--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A readonly property cannot carry a default value
--FILE--
<?php
class ReadonlyDefault {
    public readonly int $x = 1;
}
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  Readonly property ReadonlyDefault::$x cannot have default value%A
--CLEAN--
<?php
