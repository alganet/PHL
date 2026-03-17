--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
array_uintersect should report missing method when callable is an array with 2 elements
--FILE--
<?php
class Foo {}
$o = new Foo();
array_uintersect(array(1), array(1), array($o, "missingMethod"));
?>
--EXPECTF--
%s Fatal error:  Uncaught TypeError: array_uintersect(): Argument #3 must be a valid callback, class Foo does not have a method "missingMethod" in %s
--CLEAN--
<?php
unset($o);
