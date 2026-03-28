--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
Relative qualified names prepend current namespace, absolute bypass it
--FILE--
<?php
namespace B;
class Foo { function id() { return "B-Foo"; } }

namespace A\B;
class Foo { function id() { return "A-B-Foo"; } }

namespace A;
// B\Foo is relative: resolved as A\B\Foo (current NS "A" + "B\Foo")
$rel = new B\Foo();
echo $rel->id(), "\n";

// \B\Foo is absolute: resolved as B\Foo directly
$abs = new \B\Foo();
echo $abs->id(), "\n";
?>
--EXPECT--
A-B-Foo
B-Foo
--CLEAN--
<?php
