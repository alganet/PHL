--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Match expression: arms mixing int, string, bool, and null (strict semantics)
--FILE--
<?php
$describe = function ($v) {
    return match ($v) {
        null    => 'is null',
        true    => 'is bool true',
        false   => 'is bool false',
        0       => 'is int zero',
        ''      => 'is empty string',
        'hi'    => 'is greeting',
        default => 'something else',
    };
};
echo $describe(null), "\n";
echo $describe(true), "\n";
echo $describe(false), "\n";
echo $describe(0), "\n";
echo $describe(''), "\n";
echo $describe('hi'), "\n";
echo $describe('other'), "\n";
?>
--EXPECT--
is null
is bool true
is bool false
is int zero
is empty string
is greeting
something else
--CLEAN--
<?php
unset($describe);
