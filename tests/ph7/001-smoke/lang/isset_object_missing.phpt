--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
isset/empty on a missing object property is silent (no Undefined-attribute warning)
--FILE--
<?php
$o = new stdClass;
echo var_export(isset($o->missing), true), "\n";        // false  (no warning)
echo var_export(empty($o->missing), true), "\n";        // true   (no warning)
echo var_export(isset($o->missing->deep), true), "\n";  // false  (chained, no warning)
echo var_export(empty($o->missing->deep), true), "\n";  // true

class IssetMissC { public $a = 5; public $n = null; }
$c = new IssetMissC();
echo var_export(isset($c->a), true), "\n";              // true
echo var_export(isset($c->n), true), "\n";              // false (null prop)
echo var_export(empty($c->a), true), "\n";              // false
echo var_export(isset($c->missing), true), "\n";        // false (no warning)

$c->a = new stdClass; $c->a->b = 1;
echo var_export(isset($c->a->b), true), "\n";           // true
echo var_export(isset($c->a->c), true), "\n";           // false (no warning)
?>
--EXPECT--
false
true
false
true
true
false
false
false
true
false
--CLEAN--
<?php
