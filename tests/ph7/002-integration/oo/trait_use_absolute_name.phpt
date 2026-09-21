--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
a TRAIT body's `use` accepts qualified and fully-qualified trait names
--DESCRIPTION--
The trait-body use parser read a single identifier and choked on the leading
'\' of `use \Ns\Other;` (the class-body parser was already fixed). A trait name
is a full class reference in both bodies. (Adaptation blocks `use T { ... }`
inside a TRAIT body remain unsupported.)
--FILE--
<?php
namespace Dep {
    trait TuHelper { public function help() { return "helped"; } }
    trait TuOther { public function more() { return "more"; } }
}
namespace Main {
    use Dep\TuOther;
    trait TuMid { use \Dep\TuHelper, TuOther; }
    class TuUses { use TuMid; }
    $o = new TuUses;
    echo $o->help(), " ", $o->more(), "\n";
}
?>
--EXPECT--
helped more
--CLEAN--
<?php
