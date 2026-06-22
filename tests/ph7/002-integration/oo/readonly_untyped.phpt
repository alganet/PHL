--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A readonly property must have a declared type
--FILE--
<?php
class ReadonlyUntyped {
    public readonly $x;
}
echo "unreached";
?>
--EXPECTF--
%s Fatal error:  Readonly property ReadonlyUntyped::$x must have type%A
--CLEAN--
<?php
