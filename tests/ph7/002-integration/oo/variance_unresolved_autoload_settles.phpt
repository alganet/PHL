--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An autoloader registered before an override supplies the classes its variance check needs
--DESCRIPTION--
Nothing is loaded while the file compiles, so the interface property and the
method are both left open. The declaration statement runs after the
autoloader is registered, which answers both and lets the class link.
--FILE--
<?php
spl_autoload_register(function ($c) {
    echo "load $c\n";
    if ($c === 'VuAsBase') {
        eval('class VuAsBase {}');
    } elseif ($c === 'VuAsDerived') {
        eval('class VuAsDerived extends VuAsBase {}');
    }
});
interface VuAsIface {
    public VuAsDerived $p { get; }
}
class VuAsParent {
    function f(VuAsDerived $a): VuAsBase { return new VuAsBase; }
}
echo "before\n";
class VuAsImpl extends VuAsParent implements VuAsIface {
    public VuAsDerived $p;
    function f(VuAsBase $a): VuAsDerived { return new VuAsDerived; }
}
echo get_class((new VuAsImpl)->f(new VuAsDerived)), "\n";
?>
--EXPECT--
before
load VuAsDerived
load VuAsBase
VuAsDerived
