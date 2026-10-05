--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
An override widening an unloaded class return type with null is refused
--DESCRIPTION--
The return class is not loaded, but no class can stand for null, so the
child's return type is wider than the parent's whatever the class turns out
to be: php's ordinary incompatible-declaration fatal.
--FILE--
<?php
class MouParent {
    function f(): MouNowhere {}
}
class MouKid extends MouParent {
    function f(): ?MouNowhere {}
}
echo "unreached\n";
?>
--EXPECTF--
%s Fatal error:  Declaration of MouKid::f(): ?MouNowhere must be compatible with MouParent::f(): MouNowhere in %s on line 6%A
