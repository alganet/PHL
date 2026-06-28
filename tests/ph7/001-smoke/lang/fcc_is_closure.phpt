--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A first-class callable is a Closure object (instanceof / is_callable / gettype)
--FILE--
<?php
function fcc_ic($x){ return $x; }
$f = fcc_ic(...);
echo var_export($f instanceof Closure, true), "\n";
echo var_export(is_callable($f), true), "\n";
echo gettype($f), "\n";
$g = strlen(...);
echo var_export($g instanceof Closure, true), "\n";
?>
--EXPECT--
true
true
object
true
--CLEAN--
<?php
