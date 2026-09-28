--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
static::class in a property default is its own compile-time fatal
--FILE--
<?php
class CesB {
    public $c = static::class;
}
echo "unreachable\n";
?>
--EXPECTF--
%Astatic::class cannot be used for compile-time class name resolution%A
--CLEAN--
<?php
