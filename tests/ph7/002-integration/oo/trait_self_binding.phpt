--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
self:: in trait method resolves to the using class
--FILE--
<?php
trait Identifier {
    public function identify() { return self::class; }
}
class Foo { use Identifier; }
class Bar { use Identifier; }
$f = new Foo();
$b = new Bar();
echo $f->identify() . "\n";
echo $b->identify() . "\n";
?>
--EXPECT--
Foo
Bar
--CLEAN--
<?php
