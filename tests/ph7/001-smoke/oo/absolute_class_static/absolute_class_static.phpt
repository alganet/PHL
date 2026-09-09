--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A leading-backslash class name in :: is absolute inside a namespace (\Closure::bind)
--FILE--
<?php
namespace AbsNsTest\Deep;
class AbsHelper { public static function tag() { return "ns-helper"; } }
$out = [];
$out[] = \Closure::fromCallable('strlen')("hello");
$out[] = \Closure::bind(static function ($a, $b) { return $a * $b; }, null)(4, 5);
$out[] = \stdClass::class;
$out[] = \Exception::class;
$out[] = \Closure::class;
$out[] = AbsHelper::class;
$out[] = AbsHelper::tag();
$out[] = \strlen("abc");
echo implode("\n", array_map(fn($x) => (string)$x, $out)), "\n";
--EXPECT--
5
20
stdClass
Exception
Closure
AbsNsTest\Deep\AbsHelper
ns-helper
3
--CLEAN--
<?php
