--CREDITS--
SPDX-FileCopyrightText: 2026 Alexandre Gomes Gaigalas <alganet@gmail.com>
SPDX-License-Identifier: BSD-3-Clause
--TEST--
A fresh object property is auto-created by array-append / keyed-write / ??= (not just plain =)
--FILE--
<?php
/* Only pure-write forms are asserted here: PHP emits an "Undefined property"
 * warning for the read-modify-write forms ($o->n++, $o->s .= "x", $o->c += 1) on
 * a fresh property that PHL does not, so those are covered by spot-checks instead.
 * The value/auto-creation behaviour is identical across all forms. */

/* stdClass: array-append, keyed write, nested write, ??= (subscript and plain member) */
$o = new stdClass;
$o->arr[] = 5; $o->arr[] = 6;
echo json_encode($o), "\n";                         // {"arr":[5,6]}

$o = new stdClass;
$o->map["k"] = 7;
$k = "x"; $o->kv[$k] = 9;
echo json_encode($o), "\n";                         // {"map":{"k":7},"kv":{"x":9}}

$o = new stdClass;
$o->grid[1][2] = 9;                                 // nested auto-vivify
echo json_encode($o), "\n";                         // {"grid":{"1":{"2":9}}}

$o = new stdClass;
$o->box["a"] ??= 1;                                 // ??= on a missing subscript prop
$o->flag ??= true;                                  // ??= on a missing plain prop
echo json_encode($o), "\n";                         // {"box":{"a":1},"flag":true}

/* a DECLARED property that was unset() is recreated by the same forms */
class DpvC { public $p; public $q; }
$c = new DpvC();
unset($c->p, $c->q);
$c->p[] = 5;
$c->q["k"] = 7;
echo json_encode($c), "\n";                         // {"p":[5],"q":{"k":7}}

/* a declared property recreated after unset() is UNDEFINED, not its class default:
 * ??= must see null and assign (it must not keep the old default value) */
class DpvDef { public $n = 10; }
$d = new DpvDef();
unset($d->n);
$d->n ??= 99;
echo json_encode($d), "\n";                         // {"n":99}  (not {"n":10})

/* negatives: a lookup must NOT create the property */
$o = new stdClass;
echo var_export(isset($o->missing), true), "\n";    // false
echo var_export(empty($o->missing), true), "\n";    // true
echo json_encode($o), "\n";                         // {}  (still empty)
?>
--EXPECT--
{"arr":[5,6]}
{"map":{"k":7},"kv":{"x":9}}
{"grid":{"1":{"2":9}}}
{"box":{"a":1},"flag":true}
{"p":[5],"q":{"k":7}}
{"n":99}
false
true
{}
--CLEAN--
<?php
