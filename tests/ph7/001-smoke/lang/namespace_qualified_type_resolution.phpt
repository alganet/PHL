--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A qualified name in a type declaration resolves like every other name
--DESCRIPTION--
Types used to be stored and compared AS WRITTEN, so `: Sub\Thing` inside `namespace App;`
checked against the literal `Sub\Thing` while the value's class was `App\Sub\Thing`, and every
qualified type face in a namespaced tree raised a TypeError. A qualified type now resolves
exactly as `new Sub\Thing` does: the leading segment through the `use` imports, else the
current namespace prefix; only an absolute `\X` stays as written.
--FILE--
<?php
namespace NqtrApp\Sub { class Thing {} class Other {} interface Face {} }
namespace NqtrOut { class Far {} }
namespace NqtrApp {
    use NqtrApp\Sub as S;
    use NqtrOut\Far as F;
    function nqtrMake(): Sub\Thing { return new Sub\Thing(); }
    function nqtrAlias(): S\Other { return new S\Other(); }
    function nqtrTake(Sub\Thing $t): string { return \get_class($t); }
    function nqtrUnion(): Sub\Thing|S\Other { return new Sub\Other(); }
    function nqtrAbs(): \NqtrOut\Far { return new \NqtrOut\Far(); }
    class NqtrHolder {
        public ?Sub\Thing $p = null;
        public function iface(): Sub\Face|null { return null; }
        public function far(): F { return new \NqtrOut\Far(); }
    }
    echo \get_class(nqtrMake()), "\n";
    echo \get_class(nqtrAlias()), "\n";
    echo nqtrTake(new Sub\Thing()), "\n";
    echo \get_class(nqtrUnion()), "\n";
    echo \get_class(nqtrAbs()), "\n";
    $h = new NqtrHolder;
    $h->p = new Sub\Thing();
    echo \get_class($h->p), "\n";
    \var_dump($h->iface());
    echo \get_class($h->far()), "\n";
    $r = new \ReflectionFunction('NqtrApp\nqtrMake');
    echo $r->getReturnType()->getName(), "\n";
    echo (string)(new \ReflectionProperty('NqtrApp\NqtrHolder', 'p'))->getType(), "\n";
}
?>
--EXPECT--
NqtrApp\Sub\Thing
NqtrApp\Sub\Other
NqtrApp\Sub\Thing
NqtrApp\Sub\Other
NqtrOut\Far
NqtrApp\Sub\Thing
NULL
NqtrOut\Far
NqtrApp\Sub\Thing
?NqtrApp\Sub\Thing
--CLEAN--
<?php
