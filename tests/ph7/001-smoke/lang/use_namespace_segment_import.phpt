--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A `use X\Y;` segment import resolves the leading segment of a qualified name Y\Z
--DESCRIPTION--
Regression: `use App\Lib;` imports the segment `Lib` (alias -> App\Lib). A later
qualified reference `Lib\Base` must map its LEADING segment through that import
(-> App\Lib\Base), not prepend the current namespace (which produced
App\Run\Lib\Base -> "Nonexistent base class"). The literal path already did this;
the class-reference resolver used by extends/implements/new/instanceof/::class did
not. Exercises every construct that flows through GenStateResolveName.
--FILE--
<?php
namespace App\Lib {
    class Base { public function who() { return 'Base'; } }
    interface Iface {}
    class Helper {}
}
namespace App\Run {
    use App\Lib;
    class Impl extends Lib\Base implements Lib\Iface {}
    $o = new Impl();
    echo 'parent=', get_parent_class($o), "\n";
    echo 'impl=', ($o instanceof Lib\Iface ? '1' : '0'), "\n";
    echo 'new=', get_class(new Lib\Helper()), "\n";
    echo 'static=', Lib\Base::class, "\n";
}
?>
--EXPECT--
parent=App\Lib\Base
impl=1
new=App\Lib\Helper
static=App\Lib\Base
--CLEAN--
<?php
