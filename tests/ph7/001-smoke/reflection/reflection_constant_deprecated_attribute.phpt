--CREDITS--
SPDX-FileCopyrightText: 2025 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
ReflectionConstant::isDeprecated() and its export read a user #[\Deprecated]
--FILE--
<?php
namespace Foo;

// isDeprecated() and the export read a user #[\Deprecated] on a `const`
// statement, as they read the engine's own table for E_STRICT -- and reading
// either does not raise the notice that naming the constant does.
#[\Deprecated]
const A = 1;
#[\Deprecated("use B2", since: "1.0")]
const B = 2;
#[Deprecated]
const C = 3;
#[Other, \deprecated]
const D = 4;
const E = 5;
const F = 6;
define('G', 7);

foreach (['Foo\A', 'Foo\B', 'Foo\C', 'Foo\D', 'Foo\E', 'Foo\F', 'G', 'E_STRICT'] as $n) {
    $r = new \ReflectionConstant($n);
    echo $n, ': ';
    var_dump($r->isDeprecated());
    echo $r;
}
--EXPECT--
Foo\A: bool(true)
Constant [ <deprecated> int Foo\A ] { 1 }
Foo\B: bool(true)
Constant [ <deprecated> int Foo\B ] { 2 }
Foo\C: bool(false)
Constant [ int Foo\C ] { 3 }
Foo\D: bool(true)
Constant [ <deprecated> int Foo\D ] { 4 }
Foo\E: bool(false)
Constant [ int Foo\E ] { 5 }
Foo\F: bool(false)
Constant [ int Foo\F ] { 6 }
G: bool(false)
Constant [ int G ] { 7 }
E_STRICT: bool(true)
Constant [ <persistent, deprecated> int E_STRICT ] { 2048 }
