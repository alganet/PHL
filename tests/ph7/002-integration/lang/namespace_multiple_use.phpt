--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Namespace multiple use declarations
--FILE--
<?php
namespace A;
class Foo { function id() { return "A\\Foo"; } }

namespace B;
class Bar { function id() { return "B\\Bar"; } }

namespace C;
use A\Foo, B\Bar;

$f = new Foo();
$b = new Bar();
echo $f->id(), "\n";
echo $b->id(), "\n";
?>
--EXPECT--
A\Foo
B\Bar
--CLEAN--
<?php
